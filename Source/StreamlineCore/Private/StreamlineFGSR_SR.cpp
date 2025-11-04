#include "StreamlineFGSR_SR.h"
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

const TCHAR* FStreamlineFGSR_SRUpscaler::GetDebugName() const
{
	return TEXT("StreamlineFGSR_SR");
}

bool IsFGSR_SRActive()
{
	return FGSR_SRModeFromCvar() != sl::FGSR_SRMode::eOff ? true : false;
}

BEGIN_SHADER_PARAMETER_STRUCT(FFGSR_SRShaderParameters, )
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputDepth)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputVelocity)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OutputColor)
END_SHADER_PARAMETER_STRUCT()

void AddStreamlineFGSR_SRStateRenderPass(FRDGBuilder& GraphBuilder, uint32 ViewID)
{

}

void AddStreamlineFGSR_SREvaluateRenderPass(FStreamlineRHI* StreamlineRHIExtensions, FRDGBuilder& GraphBuilder, uint32 ViewID, const FIntRect& SecondaryViewRect, FRDGTextureRef SLSceneDepth, FRDGTextureRef SLSceneVelocity, FRDGTextureRef SLSceneColorWithoutHUD)
{
	FFGSR_SRShaderParameters* PassParameters = GraphBuilder.AllocParameters<FFGSR_SRShaderParameters>();
	PassParameters->InputDepth = SLSceneDepth;
	PassParameters->InputVelocity = SLSceneVelocity;
	PassParameters->OutputColor = SLSceneColorWithoutHUD;
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Streamline FGSR_SR Evaluate ViewID = %u", ViewID),
		PassParameters,
		ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull,
		[StreamlineRHIExtensions, PassParameters, ViewID, SecondaryViewRect](FRHICommandListImmediate& RHICmdList) mutable
		{
			check(PassParameters->InputDepth);
			check(PassParameters->InputVelocity);
			check(PassParameters->OutputColor);
			PassParameters->InputDepth->MarkResourceAsUsed();
			PassParameters->InputVelocity->MarkResourceAsUsed();
			PassParameters->OutputColor->MarkResourceAsUsed();
			FRHITexture* FGSR_SRInputDepth = PassParameters->InputDepth->GetRHI();
			FRHITexture* FGSR_SRInputVelocity = PassParameters->InputVelocity->GetRHI();
			FRHITexture* FGSR_SROutputColor = PassParameters->OutputColor->GetRHI();
			RHICmdList.EnqueueLambda(
				[StreamlineRHIExtensions, FGSR_SRInputDepth, FGSR_SRInputVelocity, FGSR_SROutputColor, ViewID, SecondaryViewRect](FRHICommandListImmediate& Cmd) mutable
				{
					sl::FrameToken* FrameToken = FStreamlineCoreModule::GetStreamlineRHI()->GetFrameToken(GFrameCounterRenderThread);

					TArray<FRHIStreamlineResource> FGSR_SRResource;
					FGSR_SRResource.Add({ FGSR_SRInputDepth, SecondaryViewRect, EStreamlineResource::Depth });
					FGSR_SRResource.Add({ FGSR_SRInputVelocity, SecondaryViewRect, EStreamlineResource::MotionVectors });
					FGSR_SRResource.Add({ FGSR_SROutputColor, SecondaryViewRect, EStreamlineResource::ScalingOutputColor });
					StreamlineRHIExtensions->StreamlineEvaluateFGSR_SR(Cmd, FGSR_SRResource, FrameToken, ViewID);
				});
		});
}
