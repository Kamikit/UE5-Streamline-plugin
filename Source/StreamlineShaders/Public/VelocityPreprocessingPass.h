#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "ScreenPass.h"

extern STREAMLINESHADERS_API FRDGTextureRef AddStreamlineVelocityPreProcessingPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef InSceneDepthTexture,
	FRDGTextureRef InVelocityTexture
);