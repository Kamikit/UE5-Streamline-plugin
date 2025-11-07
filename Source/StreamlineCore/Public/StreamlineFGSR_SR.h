#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "StreamlineCore.h"
#include "PostProcess/PostProcessInputs.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/TemporalAA.h"

#if (ENGINE_MAJOR_VERSION == 5) && (ENGINE_MINOR_VERSION >= 3)
#include "TemporalUpscaler.h"
using IFGSR_SRTemporalUpscaler = UE::Renderer::Private::ITemporalUpscaler;
using FGSR_SRPassInput = UE::Renderer::Private::ITemporalUpscaler::FInputs;
using FGSR_SRView = FSceneView;
using IFGSR_SRTemporalUpscalerHistory = UE::Renderer::Private::ITemporalUpscaler::IHistory;
#else
using IFGSR_SRTemporalUpscaler = ITemporalUpscaler;
using FGSR_SRPassInput = ITemporalUpscaler::FPassInputs;
using FGSR_SRView = FViewInfo;
using IFGSR_SRTemporalUpscalerHistory = ICustomTemporalAAHistory;
#endif

class FStreamlineUpscalerHistory final : public IFGSR_SRTemporalUpscalerHistory, public FRefCountBase
{
public:
	FStreamlineUpscalerHistory() { }

	virtual ~FStreamlineUpscalerHistory() { }

	const TCHAR* GetDebugName() const override
	{
		return TEXT("StreamlineFGSR_SR");
	}

	uint64 GetGPUSizeBytes() const override
	{
		return 0;
	}

	uint32 AddRef() const final
	{
		return FRefCountBase::AddRef();
	}

	uint32 Release() const final
	{
		return FRefCountBase::Release();
	}

	uint32 GetRefCount() const final
	{
		return FRefCountBase::GetRefCount();
	}

};

class FStreamlineFGSR_SRUpscaler final : public IFGSR_SRTemporalUpscaler
{
public:
	FStreamlineFGSR_SRUpscaler(FStreamlineRHI* StreamlineRHI);
	virtual ~FStreamlineFGSR_SRUpscaler();

	const TCHAR* GetDebugName() const override;

	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;
	static float GetUpsampleResolutionFraction();
	IFGSR_SRTemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;

	// @TODO_STREAMLINE: implement the upscaling pass
	FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const FGSR_SRView& SceneView,
		const FGSR_SRPassInput& PassInputs) const override;

	void SetPostProcessingInputs(FPostProcessingInputs const& NewInputs);

	static void SaveScreenPercentage();
	static void UpdateScreenPercentage();
	static void RestoreScreenPercentage();

	static void OnChangeFGSRTemporalUpscalingEnabled(IConsoleVariable* Var);

private:
	mutable FPostProcessingInputs PostInputs;
	mutable FStreamlineRHI* StreamlineRHIExtensions;

	static float SavedScreenPercentage;
};



class FStreamlineRHI;

bool IsFGSR_SRActive();

//void SetFGSROnChangedCallback(const FConsoleVariableDelegate& Callback);

class FRHICommandListImmediate;
struct FRHIStreamlineArguments;
class FSceneViewFamily;
class FRDGBuilder;
void AddStreamlineFGSR_SRStateRenderPass(FRDGBuilder& GraphBuilder, uint32 ViewID);
void AddStreamlineFGSR_SREvaluateRenderPass(FStreamlineRHI* StreamlineRHIExtensions, FRDGBuilder& GraphBuilder, uint32 ViewID, const FIntRect& SecondaryViewRect, FRDGTextureRef SLSceneDepth, FRDGTextureRef SLSceneVelocity, FRDGTextureRef SLSceneColorWithoutHUD);

