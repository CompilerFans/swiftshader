#include "Pipeline/ShaderCompiler/ShaderCompiler.hpp"

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

constexpr const char kVertexPositionAssembly[] =
    "OpCapability Shader\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Vertex %main \"main\" %inPos %gl_VertexIndex %gl_InstanceIndex %gl_PerVertex\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %inPos \"inPos\"\n"
    "OpName %gl_VertexIndex \"gl_VertexIndex\"\n"
    "OpName %gl_InstanceIndex \"gl_InstanceIndex\"\n"
    "OpName %gl_PerVertex \"gl_PerVertex\"\n"
    "OpMemberName %gl_PerVertex_t 0 \"gl_Position\"\n"
    "OpMemberName %gl_PerVertex_t 1 \"gl_PointSize\"\n"
    "OpMemberName %gl_PerVertex_t 2 \"gl_ClipDistance\"\n"
    "OpMemberName %gl_PerVertex_t 3 \"gl_CullDistance\"\n"
    "OpDecorate %inPos Location 0\n"
    "OpDecorate %gl_VertexIndex BuiltIn VertexIndex\n"
    "OpDecorate %gl_InstanceIndex BuiltIn InstanceIndex\n"
    "OpMemberDecorate %gl_PerVertex_t 0 BuiltIn Position\n"
    "OpMemberDecorate %gl_PerVertex_t 1 BuiltIn PointSize\n"
    "OpMemberDecorate %gl_PerVertex_t 2 BuiltIn ClipDistance\n"
    "OpMemberDecorate %gl_PerVertex_t 3 BuiltIn CullDistance\n"
    "OpDecorate %gl_PerVertex_t Block\n"
    "%void = OpTypeVoid\n"
    "%func = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v3float = OpTypeVector %float 3\n"
    "%v4float = OpTypeVector %float 4\n"
    "%int = OpTypeInt 32 1\n"
    "%uint = OpTypeInt 32 0\n"
    "%uint_1 = OpConstant %uint 1\n"
    "%int_0 = OpConstant %int 0\n"
    "%float_0 = OpConstant %float 0\n"
    "%float_1 = OpConstant %float 1\n"
    "%_arr_float_uint_1 = OpTypeArray %float %uint_1\n"
    "%gl_PerVertex_t = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1\n"
    "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
    "%_ptr_Input_int = OpTypePointer Input %int\n"
    "%_ptr_Output_gl_PerVertex_t = OpTypePointer Output %gl_PerVertex_t\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%inPos = OpVariable %_ptr_Input_v3float Input\n"
    "%gl_VertexIndex = OpVariable %_ptr_Input_int Input\n"
    "%gl_InstanceIndex = OpVariable %_ptr_Input_int Input\n"
    "%gl_PerVertex = OpVariable %_ptr_Output_gl_PerVertex_t Output\n"
    "%pos = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_1\n"
    "%main = OpFunction %void None %func\n"
    "%entry = OpLabel\n"
    "%pos_ptr = OpAccessChain %_ptr_Output_v4float %gl_PerVertex %int_0\n"
    "OpStore %pos_ptr %pos\n"
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

constexpr const char kConstantColorAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%float_1 = OpConstant %float 1\n"
    "%float_0 = OpConstant %float 0\n"
    "%red = OpConstantComposite %v4float %float_1 %float_0 %float_0 %float_1\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "OpStore %outColor %red\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kFragCoordAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %fragCoord %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %fragCoord \"fragCoord\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %fragCoord BuiltIn FragCoord\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
    "%fragCoord = OpVariable %_ptr_Input_v4float Input\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%coord = OpLoad %v4float %fragCoord\n"
    "OpStore %outColor %coord\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kFrontFacingAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %frontFacing %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %frontFacing \"frontFacing\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %frontFacing BuiltIn FrontFacing\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%bool = OpTypeBool\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%_ptr_Input_bool = OpTypePointer Input %bool\n"
    "%frontFacing = OpVariable %_ptr_Input_bool Input\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%float_1 = OpConstant %float 1\n"
    "%float_0 = OpConstant %float 0\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%ff = OpLoad %bool %frontFacing\n"
    "%r = OpSelect %float %ff %float_1 %float_0\n"
    "%b = OpSelect %float %ff %float_0 %float_1\n"
    "%color = OpCompositeConstruct %v4float %r %float_0 %b %float_1\n"
    "OpStore %outColor %color\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kFragCoordDiscardAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %gl_FragCoord %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %gl_FragCoord \"gl_FragCoord\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %gl_FragCoord BuiltIn FragCoord\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
    "%gl_FragCoord = OpVariable %_ptr_Input_v4float Input\n"
    "%uint = OpTypeInt 32 0\n"
    "%uint_0 = OpConstant %uint 0\n"
    "%_ptr_Input_float = OpTypePointer Input %float\n"
    "%float_640 = OpConstant %float 640\n"
    "%bool = OpTypeBool\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%float_1 = OpConstant %float 1\n"
    "%float_0 = OpConstant %float 0\n"
    "%25 = OpConstantComposite %v4float %float_1 %float_0 %float_0 %float_1\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%13 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_0\n"
    "%14 = OpLoad %float %13\n"
    "%17 = OpFOrdLessThan %bool %14 %float_640\n"
    "OpSelectionMerge %19 None\n"
    "OpBranchConditional %17 %18 %19\n"
    "%18 = OpLabel\n"
    "OpKill\n"
    "%19 = OpLabel\n"
    "OpStore %outColor %25\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kPointCoordAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %pointCoord %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %pointCoord \"pointCoord\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %pointCoord BuiltIn PointCoord\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v2float = OpTypeVector %float 2\n"
    "%_ptr_Input_v2float = OpTypePointer Input %v2float\n"
    "%pointCoord = OpVariable %_ptr_Input_v2float Input\n"
    "%v4float = OpTypeVector %float 4\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%float_0 = OpConstant %float 0\n"
    "%float_1 = OpConstant %float 1\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%pc = OpLoad %v2float %pointCoord\n"
    "%x = OpCompositeExtract %float %pc 0\n"
    "%y = OpCompositeExtract %float %pc 1\n"
    "%color = OpCompositeConstruct %v4float %x %y %float_0 %float_1\n"
    "OpStore %outColor %color\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kFlatInterpolatedColorAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %inColor %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %inColor \"inColor\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %inColor Location 0\n"
    "OpDecorate %inColor Flat\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%v3float = OpTypeVector %float 3\n"
    "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
    "%inColor = OpVariable %_ptr_Input_v3float Input\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%float_1 = OpConstant %float 1\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%c = OpLoad %v3float %inColor\n"
    "%r = OpCompositeExtract %float %c 0\n"
    "%g = OpCompositeExtract %float %c 1\n"
    "%b = OpCompositeExtract %float %c 2\n"
    "%color = OpCompositeConstruct %v4float %r %g %b %float_1\n"
    "OpStore %outColor %color\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kInterpolatedColorAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %inColor %outColor\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %inColor \"inColor\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpDecorate %inColor Location 0\n"
    "OpDecorate %outColor Location 0\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%v3float = OpTypeVector %float 3\n"
    "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
    "%inColor = OpVariable %_ptr_Input_v3float Input\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%float_1 = OpConstant %float 1\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%c = OpLoad %v3float %inColor\n"
    "%r = OpCompositeExtract %float %c 0\n"
    "%g = OpCompositeExtract %float %c 1\n"
    "%b = OpCompositeExtract %float %c 2\n"
    "%color = OpCompositeConstruct %v4float %r %g %b %float_1\n"
    "OpStore %outColor %color\n"
    "OpReturn\n"
    "OpFunctionEnd\n";

constexpr const char kInterpolatedColorFragDepthAssembly[] =
    "OpCapability Shader\n"
    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
    "OpMemoryModel Logical GLSL450\n"
    "OpEntryPoint Fragment %main \"main\" %outColor %vColor %gl_FragDepth\n"
    "OpExecutionMode %main OriginUpperLeft\n"
    "OpExecutionMode %main DepthReplacing\n"
    "OpSource GLSL 450\n"
    "OpName %main \"main\"\n"
    "OpName %outColor \"outColor\"\n"
    "OpName %vColor \"vColor\"\n"
    "OpName %gl_FragDepth \"gl_FragDepth\"\n"
    "OpDecorate %outColor Location 0\n"
    "OpDecorate %vColor Location 0\n"
    "OpDecorate %gl_FragDepth BuiltIn FragDepth\n"
    "%void = OpTypeVoid\n"
    "%3 = OpTypeFunction %void\n"
    "%float = OpTypeFloat 32\n"
    "%v4float = OpTypeVector %float 4\n"
    "%v3float = OpTypeVector %float 3\n"
    "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
    "%vColor = OpVariable %_ptr_Input_v3float Input\n"
    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
    "%_ptr_Output_float = OpTypePointer Output %float\n"
    "%gl_FragDepth = OpVariable %_ptr_Output_float Output\n"
    "%float_1 = OpConstant %float 1\n"
    "%uint = OpTypeInt 32 0\n"
    "%uint_2 = OpConstant %uint 2\n"
    "%_ptr_Input_float = OpTypePointer Input %float\n"
    "%uint_0 = OpConstant %uint 0\n"
    "%bool = OpTypeBool\n"
    "%float_0_200000003 = OpConstant %float 0.200000003\n"
    "%float_0_800000012 = OpConstant %float 0.800000012\n"
    "%main = OpFunction %void None %3\n"
    "%5 = OpLabel\n"
    "%13 = OpLoad %v3float %vColor\n"
    "%15 = OpCompositeExtract %float %13 0\n"
    "%16 = OpCompositeExtract %float %13 1\n"
    "%17 = OpCompositeExtract %float %13 2\n"
    "%18 = OpCompositeConstruct %v4float %15 %16 %17 %float_1\n"
    "OpStore %outColor %18\n"
    "%24 = OpAccessChain %_ptr_Input_float %vColor %uint_2\n"
    "%25 = OpLoad %float %24\n"
    "%27 = OpAccessChain %_ptr_Input_float %vColor %uint_0\n"
    "%28 = OpLoad %float %27\n"
    "%30 = OpFOrdGreaterThan %bool %25 %28\n"
    "%33 = OpSelect %float %30 %float_0_200000003 %float_0_800000012\n"
    "OpStore %gl_FragDepth %33\n"
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

TEST(ShaderCompiler, CompilesVertexAssemblyToCudaLikeSource)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsVertex(sw::ShaderModuleInput::fromAssembly("main", kVertexPositionAssembly),
	                                             sw::CodegenTarget::CudaLikeSource);

	EXPECT_TRUE(result.kernelIR.hasVertexLoweringInfo());
	EXPECT_NE(result.text.find("extern \"C\" __global__ void vs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("params.vertexData + vertexIndex * params.vertexStride + params.positionOffset"), std::string::npos);
}

TEST(ShaderCompiler, CompilesVertexAssemblyToLlvmIR)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsVertex(sw::ShaderModuleInput::fromAssembly("main", kVertexPositionAssembly),
	                                             sw::CodegenTarget::LlvmIR);

	EXPECT_TRUE(result.kernelIR.hasVertexLoweringInfo());
	EXPECT_NE(result.text.find("%struct.VsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @vs_entry(%struct.VsParams* %params)"), std::string::npos);
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

TEST(ShaderCompiler, EmitsConstantColorFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kConstantColorAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("@swiftshader.color_r = internal constant float 1.000000e+00"), std::string::npos);
}

TEST(ShaderCompiler, EmitsCombinedTextureFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kCombinedImageSamplerAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define internal void @sampleTexture"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
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

TEST(ShaderCompiler, EmitsConstantColorFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kConstantColorAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_FALSE(result.analysis.texturePlan.bootstrapSupported);
	EXPECT_NE(result.text.find("struct FsParams"), std::string::npos);
	EXPECT_NE(result.text.find("extern \"C\" __global__ void fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("outR = packColor(1.0f);"), std::string::npos);
	EXPECT_NE(result.text.find("outG = packColor(0.0f);"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFragCoordQuadrantsFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFragCoordAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("invocation.x * 2u < params.width"), std::string::npos);
	EXPECT_NE(result.text.find("invocation.y * 2u < params.height"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFrontFacingFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFrontFacingAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("bool frontFacing = invocation.frontFacing != 0u;"), std::string::npos);
	EXPECT_NE(result.text.find("float colorR = frontFacing ? 1.0f : 0.0f;"), std::string::npos);
	EXPECT_NE(result.text.find("float colorB = frontFacing ? 0.0f : 1.0f;"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFragCoordQuadrantsFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFragCoordAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("@swiftshader.has_texture_plan = internal constant i1 false"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFrontFacingFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFrontFacingAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("frontFacing"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFragCoordDiscardLeftConstantColorFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFragCoordDiscardAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("@swiftshader.color_r = internal constant float 1.000000e+00"), std::string::npos);
}

TEST(ShaderCompiler, EmitsPointCoordGradientFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kPointCoordAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("pointCoordX"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFlatInterpolatedColorFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFlatInterpolatedColorAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("vertexColor0R"), std::string::npos);
}

TEST(ShaderCompiler, EmitsInterpolatedColorFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kInterpolatedColorAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("barycentric0"), std::string::npos);
	EXPECT_NE(result.text.find("vertexColor2B"), std::string::npos);
}

TEST(ShaderCompiler, EmitsInterpolatedColorFragDepthFragmentLlvmIRKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kInterpolatedColorFragDepthAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::LlvmIR);

	EXPECT_NE(result.text.find("%struct.FsParams = type"), std::string::npos);
	EXPECT_NE(result.text.find("define void @fs_entry"), std::string::npos);
	EXPECT_NE(result.text.find("nearDepth"), std::string::npos);
	EXPECT_NE(result.text.find("farDepth"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFragCoordDiscardLeftConstantColorFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFragCoordDiscardAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("invocation.x * 2u < params.width"), std::string::npos);
	EXPECT_NE(result.text.find("outA = 0u;"), std::string::npos);
	EXPECT_NE(result.text.find("outR = packColor(1.0f);"), std::string::npos);
}

TEST(ShaderCompiler, EmitsPointCoordGradientFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kPointCoordAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("invocation.pointCoordX"), std::string::npos);
	EXPECT_NE(result.text.find("invocation.pointCoordY"), std::string::npos);
}

TEST(ShaderCompiler, EmitsFlatInterpolatedColorFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kFlatInterpolatedColorAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("params.vertexColor0R"), std::string::npos);
	EXPECT_NE(result.text.find("params.vertexColor0G"), std::string::npos);
	EXPECT_NE(result.text.find("params.vertexColor0B"), std::string::npos);
}

TEST(ShaderCompiler, EmitsInterpolatedColorFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kInterpolatedColorAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("invocation.barycentric0"), std::string::npos);
	EXPECT_NE(result.text.find("params.vertexColor1G"), std::string::npos);
	EXPECT_NE(result.text.find("params.vertexColor2B"), std::string::npos);
}

TEST(ShaderCompiler, EmitsInterpolatedColorFragDepthFragmentCudaKernel)
{
	sw::ShaderCompiler compiler;
	auto result = compiler.compileGraphicsFragment(sw::ShaderModuleInput::fromAssembly("main", kInterpolatedColorFragDepthAssembly),
	                                               defaultContext(),
	                                               sw::CodegenTarget::CudaLikeSource);

	EXPECT_NE(result.text.find("params.nearDepth"), std::string::npos);
	EXPECT_NE(result.text.find("params.farDepth"), std::string::npos);
	EXPECT_NE(result.text.find("outDepth = colorB > colorR ? params.nearDepth : params.farDepth;"), std::string::npos);
}
