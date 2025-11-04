#include "VelocityPreprocessingPass.h"

#include "SceneRendering.h"

class VelocityPreprocessingCS : public FGlobalShader
{
public:
	static const int ThreadgroupSizeX = 8;
	static const int ThreadgroupSizeY = 8;
	static const int ThreadgroupSizeZ = 1;

	DECLARE_GLOBAL_SHADER(VelocityPreprocessingCS);
	SHADER_USE_PARAMETER_STRUCT(VelocityPreprocessingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneVelocityTexture)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, OutputMotionVectorTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadgroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadgroupSizeY);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEZ"), ThreadgroupSizeZ);
	}
};
IMPLEMENT_GLOBAL_SHADER(VelocityPreprocessingCS, "/Plugin/Streamline/Private/VelocityPreprocessing.usf", "MainCS", SF_Compute);

FRDGTextureRef AddStreamlineVelocityPreProcessingPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef InSceneDepthTexture,
	FRDGTextureRef InVelocityTexture
)
{
	FRDGTextureDesc MotionVectorDesc =
		FRDGTextureDesc::Create2D(
			InSceneDepthTexture->Desc.Extent,
			PF_G16R16F,
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef MotionVector = GraphBuilder.CreateTexture(
		MotionVectorDesc,
		TEXT("Streamline.PreprocessedMotionVectors"));

	VelocityPreprocessingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<VelocityPreprocessingCS::FParameters>();

	PassParameters->SceneDepthTexture = GraphBuilder.CreateSRV(InSceneDepthTexture);
	PassParameters->SceneVelocityTexture = GraphBuilder.CreateSRV(InVelocityTexture);
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->OutputMotionVectorTexture = GraphBuilder.CreateUAV(MotionVector);

	TShaderMapRef<VelocityPreprocessingCS> ComputeShader(View.ShaderMap);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Streamline Velocity Preprocessing"),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(FIntVector(InSceneDepthTexture->Desc.Extent.X, InSceneDepthTexture->Desc.Extent.Y, 1),
			FIntVector(VelocityPreprocessingCS::ThreadgroupSizeX, VelocityPreprocessingCS::ThreadgroupSizeY, VelocityPreprocessingCS::ThreadgroupSizeZ))
	);

	return MotionVector;

}