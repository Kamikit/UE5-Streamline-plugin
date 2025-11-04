#pragma once

#include "CoreMinimal.h"
#include "RenderGraphDefinitions.h"
#include "StreamlineCore.h"

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
	FStreamlineFGSR_SRUpscaler();
	virtual ~FStreamlineFGSR_SRUpscaler();

	const TCHAR* GetDebugName() const override;
};



class FStreamlineRHI;

bool IsFGSR_SRActive();

class FRHICommandListImmediate;
struct FRHIStreamlineArguments;
class FSceneViewFamily;
class FRDGBuilder;
void AddStreamlineFGSR_SRStateRenderPass(FRDGBuilder& GraphBuilder, uint32 ViewID);
void AddStreamlineFGSR_SREvaluateRenderPass(FStreamlineRHI* StreamlineRHIExtensions, FRDGBuilder& GraphBuilder, uint32 ViewID, const FIntRect& SecondaryViewRect, FRDGTextureRef SLSceneDepth, FRDGTextureRef SLSceneVelocity, FRDGTextureRef SLSceneColorWithoutHUD);

