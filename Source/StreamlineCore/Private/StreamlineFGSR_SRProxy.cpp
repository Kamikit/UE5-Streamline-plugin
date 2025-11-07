#include "StreamlineFGSR_SRProxy.h"

#include "StreamlineFGSR_SR.h"

FStreamlineFGSR_SRProxy::FStreamlineFGSR_SRProxy(FStreamlineFGSR_SRUpscaler* TemporalUpscaler)
	: TemporalUpscaler(TemporalUpscaler)
{
}

FStreamlineFGSR_SRProxy::~FStreamlineFGSR_SRProxy()
{
}

const TCHAR* FStreamlineFGSR_SRProxy::GetDebugName() const
{
	return TemporalUpscaler->GetDebugName();
}

float FStreamlineFGSR_SRProxy::GetMinUpsampleResolutionFraction() const
{
	return TemporalUpscaler->GetMinUpsampleResolutionFraction();
}

float FStreamlineFGSR_SRProxy::GetMaxUpsampleResolutionFraction() const
{
	return TemporalUpscaler->GetMaxUpsampleResolutionFraction();
}

IFGSR_SRTemporalUpscaler* FStreamlineFGSR_SRProxy::Fork_GameThread(const class FSceneViewFamily& InViewFamily) const
{
	return new FStreamlineFGSR_SRProxy(TemporalUpscaler);
}

IFGSR_SRTemporalUpscaler::FOutputs FStreamlineFGSR_SRProxy::AddPasses(
	FRDGBuilder& GraphBuilder,
	const FGSR_SRView& View,
	const FGSR_SRPassInput& PassInputs) const
{
	return TemporalUpscaler->AddPasses(GraphBuilder, View, PassInputs);
}


