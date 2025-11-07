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
#else
using IFGSR_SRTemporalUpscaler = ITemporalUpscaler;
using FGSR_SRPassInput = ITemporalUpscaler::FPassInputs;
using FGSR_SRView = FViewInfo;
#endif

class FStreamlineFGSR_SRUpscaler final : public IFGSR_SRTemporalUpscaler
{
public:
	FStreamlineFGSR_SRUpscaler();
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
	void SetStreamlineRHIExtensions(FStreamlineRHI* InStreamlineRHIExtensions);

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

