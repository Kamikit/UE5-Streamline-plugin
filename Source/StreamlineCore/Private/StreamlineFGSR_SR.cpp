#include "StreamlineFGSR_SR.h"
#include "StreamlineFGSR_SRProxy.h"
#include "StreamlineCore.h"
#include "StreamlineCorePrivate.h"
#include "StreamlineAPI.h"
#include "StreamlineRHI.h"
#include "sl_helpers.h"
#include "sl_fgsr_sr.h"
#include "UIHintExtractionPass.h"

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "RenderGraphBuilder.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ScenePrivate.h"
#include "SystemTextures.h"
#include "HAL/PlatformApplicationMisc.h"

#include "VelocityPreProcessingPass.h"

float FStreamlineFGSR_SRUpscaler::SavedScreenPercentage{ 100.0f };

static TAutoConsoleVariable<int32> CVarStreamlineFGSR_SREnable(
	TEXT("r.Streamline.FGSR_SR.Enable"),
	0,
	TEXT("FGSR_SR mode(default = 0)\n")
	TEXT("0: off\n")
	TEXT("1: always on\n"),
	ECVF_Default);

static sl::FGSR_SRMode FGSR_SRModeFromCvar()
{
	int32 FGSR_SRMode = CVarStreamlineFGSR_SREnable.GetValueOnAnyThread();
	switch (FGSR_SRMode)
	{
	case 0:
		return sl::FGSR_SRMode::eOff;
	case 1:
		return sl::FGSR_SRMode::eOn;
	default:
		UE_LOG(LogStreamline, Error, TEXT("Invalid r.Streamline.FGSR_SR.Enable value %d"), FGSR_SRMode);
		return sl::FGSR_SRMode::eOff;
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FFGSR_SRShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputColor)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputVelocity)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OutputColor)
END_SHADER_PARAMETER_STRUCT()

FStreamlineFGSR_SRUpscaler::FStreamlineFGSR_SRUpscaler(FStreamlineRHI* StreamlineRHI) : StreamlineRHIExtensions(StreamlineRHI)
{
	FMemory::Memzero(PostInputs);

	FConsoleVariableDelegate EnabledChangedDelegate = FConsoleVariableDelegate::CreateStatic(&FStreamlineFGSR_SRUpscaler::OnChangeFGSRTemporalUpscalingEnabled);
	CVarStreamlineFGSR_SREnable->SetOnChangedCallback(EnabledChangedDelegate);

	if (IsFGSR_SRActive())
	{
		SaveScreenPercentage();
		UpdateScreenPercentage();
	}
}

// @TODO_STREAMLINE: release resources
FStreamlineFGSR_SRUpscaler::~FStreamlineFGSR_SRUpscaler()
{
	
}

void FStreamlineFGSR_SRUpscaler::SetPostProcessingInputs(FPostProcessingInputs const& NewInputs)
{
	PostInputs = NewInputs;
}

void FStreamlineFGSR_SRUpscaler::SaveScreenPercentage()
{
	SavedScreenPercentage = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"))->GetValueOnGameThread();
}

void FStreamlineFGSR_SRUpscaler::UpdateScreenPercentage()
{
	float UpsampleResolutionFraction = GetUpsampleResolutionFraction();
	static IConsoleVariable* CVarScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CVarScreenPercentage->Set(UpsampleResolutionFraction * 100.0f);
}

void FStreamlineFGSR_SRUpscaler::RestoreScreenPercentage()
{
	static IConsoleVariable* CVarScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	CVarScreenPercentage->Set(SavedScreenPercentage);
}

void FStreamlineFGSR_SRUpscaler::OnChangeFGSRTemporalUpscalingEnabled(IConsoleVariable* Var)
{
	if (IsFGSR_SRActive())
	{
		SaveScreenPercentage();
		UpdateScreenPercentage();
	}
	else
	{
		RestoreScreenPercentage();
	}
}

const TCHAR* FStreamlineFGSR_SRUpscaler::GetDebugName() const
{
	return TEXT("StreamlineFGSR_SR");
}

float FStreamlineFGSR_SRUpscaler::GetMinUpsampleResolutionFraction() const
{
	return 0.5f;
}

float FStreamlineFGSR_SRUpscaler::GetMaxUpsampleResolutionFraction() const
{
	return 0.5f;
}

float FStreamlineFGSR_SRUpscaler::GetUpsampleResolutionFraction()
{
	return 0.5f;
}

IFGSR_SRTemporalUpscaler* FStreamlineFGSR_SRUpscaler::Fork_GameThread(const class FSceneViewFamily& InViewFamily) const
{
	IStreamlineModuleInterface& StreamlineCoreModule = FModuleManager::GetModuleChecked<IStreamlineModuleInterface>("StreamlineCore");
	return new FStreamlineFGSR_SRProxy(StreamlineCoreModule.GetStreamlineTemporalUpscaler());
}

IFGSR_SRTemporalUpscaler::FOutputs FStreamlineFGSR_SRUpscaler::AddPasses(
	FRDGBuilder& GraphBuilder,
	const FGSR_SRView& SceneView,
	const FGSR_SRPassInput& PassInputs) const
{
	FOutputs Outputs;
	
	const FViewInfo& View = (FViewInfo&)(SceneView);

	// @TODO_STREAMLINE: Get Input extents and output extents (SecondaryViewRect?)
	FIntPoint InputExtents = View.ViewRect.Size();
	FIntPoint InputExtentsQuantized;
	FIntPoint OutputExtents = View.GetSecondaryViewRectSize();
	FIntPoint OutputExtentsQuantized;

	// @CHECK_STREAMLINE: Set common constants
	FRHIStreamlineArguments StreamlineArguments = {};
	FMemory::Memzero(&StreamlineArguments, sizeof(StreamlineArguments));

	uint32 FrameId = GFrameCounterRenderThread;
	uint32 ViewId = HasViewIdOverride() ? 0 : View.GetViewKey();

	StreamlineArguments.FrameId = FrameId;
	StreamlineArguments.ViewId = ViewId;

	StreamlineArguments.bReset = SceneView.bCameraCut;

	StreamlineArguments.bIsDepthInverted = true; // @TODO_STREAMLINE: check if depth is inverted

	StreamlineArguments.JitterOffset = PassInputs.TemporalJitterPixels;

	StreamlineArguments.CameraNear = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.CustomCameraNearPlane"))->GetFloat();
	StreamlineArguments.CameraFar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.CustomCameraFarPlane"))->GetFloat();
	StreamlineArguments.CameraFOV = View.FOV;
	StreamlineArguments.CameraAspectRatio = float(View.ViewRect.Width()) / float(View.ViewRect.Height());

	const float MotionVectorScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.MotionVectorScale"))->GetFloat();
	const bool bDilateMotionVectors = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DilateMotionVectors"))->GetBool() != 0;
	if (bDilateMotionVectors)
	{
		StreamlineArguments.MotionVectorScale = { MotionVectorScale / View.GetSecondaryViewRectSize().X, MotionVectorScale / View.GetSecondaryViewRectSize().Y };
	}
	else
	{
		StreamlineArguments.MotionVectorScale = { MotionVectorScale / View.ViewRect.Width() , MotionVectorScale / View.ViewRect.Height() };
	}
	StreamlineArguments.bAreMotionVectorsDilated = bDilateMotionVectors;

	FViewUniformShaderParameters ViewUniformShaderParameters = *View.CachedViewUniformShaderParameters;

	StreamlineArguments.bIsOrthographicProjection = !SceneView.IsPerspectiveProjection();
	StreamlineArguments.ClipToCameraView = ViewUniformShaderParameters.ClipToView;
	StreamlineArguments.ClipToLenseClip = FRHIStreamlineArguments::FMatrix44f::Identity;
	StreamlineArguments.ClipToPrevClip = ViewUniformShaderParameters.ClipToPrevClip;
	StreamlineArguments.PrevClipToClip = ViewUniformShaderParameters.ClipToPrevClip.Inverse();

#if ENGINE_MAJOR_VERSION == 5
#if ENGINE_MINOR_VERSION >= 4
	StreamlineArguments.CameraOrigin = ViewUniformShaderParameters.ViewOriginLow;
#else
	StreamlineArguments.CameraOrigin = ViewUniformShaderParameters.RelativeWorldCameraOrigin;
#endif
#else
	StreamlineArguments.CameraOrigin = ViewUniformShaderParameters.WorldCameraOrigin;
#endif
	StreamlineArguments.CameraUp = ViewUniformShaderParameters.ViewUp;
	StreamlineArguments.CameraRight = ViewUniformShaderParameters.ViewRight;
	StreamlineArguments.CameraForward = ViewUniformShaderParameters.ViewForward;
	StreamlineArguments.CameraViewToClip = ViewUniformShaderParameters.ViewToClip;

	StreamlineArguments.CameraPinholeOffset = FRHIStreamlineArguments::FVector2f::ZeroVector;

	sl::FGSR_SRConstants FGSR_SRConstants = {};

	QuantizeSceneBufferSize(InputExtents, InputExtentsQuantized);
	QuantizeSceneBufferSize(OutputExtents, OutputExtentsQuantized);
	//UE_LOG(LogStreamline, Warning, TEXT("LR Size=%d %d"), InputExtentsQuantized.X, InputExtentsQuantized.Y);
	//UE_LOG(LogStreamline, Warning, TEXT("HR Size=%d %d"), OutputExtentsQuantized.X, OutputExtentsQuantized.Y);

	FGSR_SRConstants.mode = sl::FGSR_SRMode::eOn;
	FGSR_SRConstants.renderExtents = sl::float2(float(InputExtents.X), float(InputExtents.Y));
	FGSR_SRConstants.presentationExtents = sl::float2(float(OutputExtents.X), float(OutputExtents.Y));
	

	{
		FFGSR_SRShaderParameters* PassParameters = GraphBuilder.AllocParameters<FFGSR_SRShaderParameters>();

		FStreamlineRHI* LocalStreamlineRHIExtensions = this->StreamlineRHIExtensions;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("Streamline Common %dx%d FrameId=%u ViewID=%u", View.ViewRect.Width(), View.ViewRect.Height(), StreamlineArguments.FrameId, StreamlineArguments.ViewId),
			PassParameters,
			ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull,
			[LocalStreamlineRHIExtensions, PassParameters, StreamlineArguments, FGSR_SRConstants, ViewId](FRHICommandListImmediate& RHICmdList) mutable
			{

				// first the constants
				RHICmdList.EnqueueLambda(
					[LocalStreamlineRHIExtensions, StreamlineArguments](FRHICommandListImmediate& Cmd) mutable
					{
						LocalStreamlineRHIExtensions->SetStreamlineData(Cmd, StreamlineArguments);
					});
				RHICmdList.EnqueueLambda(
					[FGSR_SRConstants, ViewId](FRHICommandListImmediate& Cmd) mutable
					{
						CALL_SL_FEATURE_FN(sl::kFeatureFGSR_SR, slSetFGSR_SRConstants, &FGSR_SRConstants, 0, ViewId);
					});
			});
	}

	// @TODO_STREAMLINE: Tag Resources for streamline (maybe in evaluate pass?)
	FRDGTextureRef SceneColor = PassInputs.SceneColor.Texture;
	FRDGTextureRef SceneDepth = PassInputs.SceneDepth.Texture;
	FRDGTextureRef VelocityTexture = PassInputs.SceneVelocity.Texture;

	FRDGTextureRef InputMotionVector = AddStreamlineVelocityPreProcessingPass(GraphBuilder, View, SceneDepth, VelocityTexture);

	static const auto CVarPostPropagateAlpha = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PostProcessing.PropagateAlpha"));
	const bool bSupportsAlpha = (CVarPostPropagateAlpha && CVarPostPropagateAlpha->GetValueOnRenderThread() != 0);
	EPixelFormat OutputFormat = (bSupportsAlpha) ? PF_FloatRGBA : PF_FloatR11G11B10;
	FRDGTextureDesc OutputColorDesc = FRDGTextureDesc::Create2D(OutputExtentsQuantized, OutputFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputColorDesc, TEXT("StreamlineFGSR_SROutputTexture"));
	AddDrawTexturePass(GraphBuilder, View, SceneColor, OutputTexture, FIntPoint::ZeroValue, FIntPoint::ZeroValue, FIntPoint::ZeroValue);

	Outputs.FullRes.Texture = OutputTexture;
	Outputs.FullRes.ViewRect = FIntRect(FIntPoint::ZeroValue, View.GetSecondaryViewRectSize());

	// @TODO_STREAMLINE: call streamline FGSR_SR evaluate pass
	AddStreamlineFGSR_SREvaluateRenderPass(StreamlineRHIExtensions, GraphBuilder, ViewId, FIntRect(FIntPoint::ZeroValue, View.GetSecondaryViewRectSize()), SceneColor, SceneDepth, InputMotionVector, OutputTexture);

	Outputs.NewHistory = new FStreamlineUpscalerHistory();

	return Outputs;
}

bool IsFGSR_SRActive()
{
	return FGSR_SRModeFromCvar() != sl::FGSR_SRMode::eOff ? true : false;
}

//void SetFGSROnChangedCallback(const FConsoleVariableDelegate& Callback)
//{
//	CVarStreamlineFGSR_SREnable->SetOnChangedCallback(Callback);
//}

void AddStreamlineFGSR_SRStateRenderPass(FRDGBuilder& GraphBuilder, uint32 ViewID)
{

}

void AddStreamlineFGSR_SREvaluateRenderPass(FStreamlineRHI* StreamlineRHIExtensions, FRDGBuilder& GraphBuilder, uint32 ViewID, const FIntRect& SecondaryViewRect, FRDGTextureRef SLInputColor, FRDGTextureRef SLSceneDepth, FRDGTextureRef SLSceneVelocity, FRDGTextureRef SLSceneColorWithoutHUD)
{
	FFGSR_SRShaderParameters* PassParameters = GraphBuilder.AllocParameters<FFGSR_SRShaderParameters>();
	PassParameters->InputColor = SLInputColor;
	PassParameters->InputDepth = SLSceneDepth;
	PassParameters->InputVelocity = SLSceneVelocity;
	PassParameters->OutputColor = SLSceneColorWithoutHUD;
	// UE_LOG(LogStreamline, Warning, TEXT("Input Depth Size=%d %d"), SLSceneDepth->Desc.Extent.X, SLSceneDepth->Desc.Extent.Y);
	// UE_LOG(LogStreamline, Warning, TEXT("SecondaryViewRect=%d,%d -> %d,%d"), SecondaryViewRect.Min.X, SecondaryViewRect.Min.Y, SecondaryViewRect.Max.X, SecondaryViewRect.Max.Y);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Streamline FGSR_SR Evaluate ViewID = %u", ViewID),
		PassParameters,
		ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull,
		[StreamlineRHIExtensions, PassParameters, ViewID, SecondaryViewRect](FRHICommandListImmediate& RHICmdList) mutable
		{
			check(PassParameters->InputColor);
			check(PassParameters->InputDepth);
			check(PassParameters->InputVelocity);
			check(PassParameters->OutputColor);
			PassParameters->InputColor->MarkResourceAsUsed();
			PassParameters->InputDepth->MarkResourceAsUsed();
			PassParameters->InputVelocity->MarkResourceAsUsed();
			PassParameters->OutputColor->MarkResourceAsUsed();
			FRHITexture* FGSR_SRInputColor = PassParameters->InputColor->GetRHI();
			FRHITexture* FGSR_SRInputDepth = PassParameters->InputDepth->GetRHI();
			FRHITexture* FGSR_SRInputVelocity = PassParameters->InputVelocity->GetRHI();
			FRHITexture* FGSR_SROutputColor = PassParameters->OutputColor->GetRHI();
			RHICmdList.EnqueueLambda(
				[StreamlineRHIExtensions, FGSR_SRInputColor, FGSR_SRInputDepth, FGSR_SRInputVelocity, FGSR_SROutputColor, ViewID, SecondaryViewRect](FRHICommandListImmediate& Cmd) mutable
				{
					sl::FrameToken* FrameToken = FStreamlineCoreModule::GetStreamlineRHI()->GetFrameToken(GFrameCounterRenderThread);

					TArray<FRHIStreamlineResource> FGSR_SRResource;
					FGSR_SRResource.Add({ FGSR_SRInputColor, SecondaryViewRect, EStreamlineResource::ScalingInputColor });
					FGSR_SRResource.Add({ FGSR_SRInputDepth, SecondaryViewRect, EStreamlineResource::Depth });
					FGSR_SRResource.Add({ FGSR_SRInputVelocity, SecondaryViewRect, EStreamlineResource::MotionVectors });
					FGSR_SRResource.Add({ FGSR_SROutputColor, SecondaryViewRect, EStreamlineResource::ScalingOutputColor });
					StreamlineRHIExtensions->StreamlineEvaluateFGSR_SR(Cmd, FGSR_SRResource, FrameToken, ViewID);
				});
		});
}