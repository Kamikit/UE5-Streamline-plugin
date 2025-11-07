#pragma once

#include "Engine/Engine.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessUpscale.h"
#include "PostProcess/TemporalAA.h"

#include "StreamlineFGSR_SR.h"

class FStreamlineFGSR_SRProxy : public IFGSR_SRTemporalUpscaler
{
public:
	FStreamlineFGSR_SRProxy(FStreamlineFGSR_SRUpscaler* TemporalUpscaler);
	virtual ~FStreamlineFGSR_SRProxy();
	const TCHAR* GetDebugName() const override;
	float GetMinUpsampleResolutionFraction() const override;
	float GetMaxUpsampleResolutionFraction() const override;

	IFGSR_SRTemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& InViewFamily) const override;

	FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const FGSR_SRView& View,
		const FGSR_SRPassInput& PassInputs) const override;

private:
	FStreamlineFGSR_SRUpscaler* TemporalUpscaler;
};