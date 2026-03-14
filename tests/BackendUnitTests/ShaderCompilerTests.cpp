#include "Pipeline/ShaderCompiler.hpp"

#include <gtest/gtest.h>
#include "spirv-tools/libspirv.hpp"

namespace {

std::vector<uint32_t> compileSpirv(const char *assembly, spv_target_env env = SPV_ENV_VULKAN_1_0)
{
	spvtools::SpirvTools core(env);
	core.SetMessageConsumer([](spv_message_level_t, const char *, const spv_position_t &position, const char *message) {
		FAIL() << position.line << ":" << position.column << ": " << message;
	});

	std::vector<uint32_t> spirv;
	EXPECT_TRUE(core.Assemble(assembly, &spirv));
	EXPECT_TRUE(core.Validate(spirv));
	return spirv;
}

sw::ShaderCompilerAnalysisContext defaultContext()
{
	sw::ShaderCompilerAnalysisContext context = {};
	context.descriptorSetCount = 1;
	context.dynamicOffsetCount = 0;
	context.pushConstantSize = vk::MAX_PUSH_CONSTANT_SIZE;
	context.queryDescriptorBindingInfo = [](const void *, uint32_t descriptorSet, uint32_t binding, sw::ShaderDescriptorBindingInfo *bindingInfo) {
		if(bindingInfo == nullptr || descriptorSet != 0 || binding != 1)
		{
			return false;
		}

		bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindingInfo->descriptorCount = 1;
		return true;
	};
	return context;
}

constexpr const char kCombinedImageSamplerAssembly[] =
    "OpCapability Shader\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %inTexCoord %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpDecorate %inTexCoord Location 0\n"
    "OpDecorate %outColor Location 0\n"
    "OpDecorate %tex DescriptorSet 0\n"
    "OpDecorate %tex Binding 1\n"
    "%void = OpTypeVoid\n"
    "%func = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v2float = OpTypeVector %float 2\n"
    "%v4float = OpTypeVector %float 4\n"
    "%image = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
    "%sampledImage = OpTypeSampledImage %image\n"
    "%ptrInputV2 = OpTypePointer Input %v2float\n"
    "%ptrOutputV4 = OpTypePointer Output %v4float\n"
    "%ptrUniformConstantSampledImage = OpTypePointer UniformConstant %sampledImage\n"
    "%inTexCoord = OpVariable %ptrInputV2 Input\n"
    "%outColor = OpVariable %ptrOutputV4 Output\n"
    "%tex = OpVariable %ptrUniformConstantSampledImage UniformConstant\n"
    "%main = OpFunction %void None %func\n"
    "%entry = OpLabel\n"
    "%coord = OpLoad %v2float %inTexCoord\n"
    "%sampler = OpLoad %sampledImage %tex\n"
    "%color = OpImageSampleImplicitLod %v4float %sampler %coord\n"
    "OpStore %outColor %color\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kSeparateImageSamplerAssembly[] =
    "OpCapability Shader\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %outColor %inTexCoord\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpName %tex \"tex\"\n"
    "OpName %samp \"samp\"\n"
    "OpName %inTexCoord \"inTexCoord\"\n"
    "OpDecorate %outColor Location 0\n"
    "OpDecorate %tex DescriptorSet 0\n"
    "OpDecorate %tex Binding 0\n"
    "OpDecorate %samp DescriptorSet 0\n"
    "OpDecorate %samp Binding 1\n"
    "OpDecorate %inTexCoord Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%10 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
    "%_ptr_UniformConstant_10 = OpTypePointer UniformConstant %10\n"
    "%tex = OpVariable %_ptr_UniformConstant_10 UniformConstant\n"
    "%14 = OpTypeSampler\n"
    "%_ptr_UniformConstant_14 = OpTypePointer UniformConstant %14\n"
    "%samp = OpVariable %_ptr_UniformConstant_14 UniformConstant\n"
    "%18 = OpTypeSampledImage %10\n"
    "%v2float = OpTypeVector %float 2\n"
    "%_ptr_Input_v2float = OpTypePointer Input %v2float\n"
    "%inTexCoord = OpVariable %_ptr_Input_v2float Input\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%13 = OpLoad %10 %tex\n"
    "%17 = OpLoad %14 %samp\n"
    "%19 = OpSampledImage %18 %13 %17\n"
    "%23 = OpLoad %v2float %inTexCoord\n"
    "%24 = OpImageSampleImplicitLod %v4float %19 %23\n"
    "OpStore %outColor %24\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

sw::ShaderCompilerAnalysisContext separateSamplerContext()
{
	sw::ShaderCompilerAnalysisContext context = {};
	context.descriptorSetCount = 1;
	context.dynamicOffsetCount = 0;
	context.pushConstantSize = vk::MAX_PUSH_CONSTANT_SIZE;
	context.queryDescriptorBindingInfo = [](const void *, uint32_t descriptorSet, uint32_t binding, sw::ShaderDescriptorBindingInfo *bindingInfo) {
		if(bindingInfo == nullptr || descriptorSet != 0)
		{
			return false;
		}
		if(binding == 0)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			bindingInfo->descriptorCount = 1;
			return true;
		}
		if(binding == 1)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			bindingInfo->descriptorCount = 1;
			return true;
		}
		return false;
	};
	return context;
}

}  // namespace

TEST(ShaderCompiler, CompilesFragmentAssemblyToCudaLikeSource)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kCombinedImageSamplerAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_EQ(result.analysis.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
	EXPECT_TRUE(result.kernelIR.compilerAnalysisInfo().hasTexturePlan);
	EXPECT_NE(result.text.find("extern \"C\" __global__ void fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("struct FsParams"), std::string::npos);
	EXPECT_NE(result.text.find("swiftshader_fragment_feature_mask"), std::string::npos);
	EXPECT_NE(result.text.find("swiftshader_has_texture_plan"), std::string::npos);
}

TEST(ShaderCompiler, EmitsTextureFragmentCudaKernelForSupportedCombinedSamplerPath)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kCombinedImageSamplerAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_TRUE(result.analysis.texturePlan.bootstrapSupported);
	EXPECT_NE(result.text.find("struct FsParams"), std::string::npos);
	EXPECT_NE(result.text.find("sampleTexture(const FsParams &params"), std::string::npos);
	EXPECT_NE(result.text.find("extern \"C\" __global__ void fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("params.textureData"), std::string::npos);
}

TEST(ShaderCompiler, CompilesFragmentBinaryToLlvmIR)
{
	const auto words = compileSpirv(kCombinedImageSamplerAssembly);
	const sw::SpirvBinary spirv(words.data(), static_cast<uint32_t>(words.size()));

	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromBinary("main", spirv),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_EQ(result.analysis.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
	EXPECT_TRUE(result.kernelIR.compilerAnalysisInfo().hasTexturePlan);
	EXPECT_NE(result.text.find("@swiftshader.fragment_feature_mask = internal constant i32 0"), std::string::npos);
	EXPECT_NE(result.text.find("@swiftshader.has_texture_plan = internal constant i1 true"), std::string::npos);
}

TEST(ShaderCompiler, EmitsTextureFragmentCudaKernelForSupportedSeparateSamplerPath)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kSeparateImageSamplerAssembly),
	                                               separateSamplerContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_EQ(result.analysis.texturePlan.resourceKind, sw::ShaderTextureResourceKind::SeparateImageSampler);
	EXPECT_TRUE(result.analysis.texturePlan.bootstrapSupported);
	EXPECT_NE(result.text.find("struct FsParams"), std::string::npos);
	EXPECT_NE(result.text.find("extern \"C\" __global__ void fs_entry"), std::string::npos);
}
