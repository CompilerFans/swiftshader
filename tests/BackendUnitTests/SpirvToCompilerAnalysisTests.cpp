#include "Pipeline/SpirvBinary.hpp"
#include "Pipeline/ShaderCompiler/ShaderCompilerAnalysis.hpp"
#include "Pipeline/ShaderCompiler/ShaderModuleInput.hpp"

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

sw::SpirvBinary combinedImageSamplerFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
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

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary separateImageSamplerFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
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

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary storageImageWriteFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpName %main \"main\"\n"
	    "OpDecorate %outColor Location 0\n"
	    "OpDecorate %storageImage DescriptorSet 0\n"
	    "OpDecorate %storageImage Binding 2\n"
	    "%void = OpTypeVoid\n"
	    "%func = OpTypeFunction %void\n"
	    "%float = OpTypeFloat 32\n"
	    "%int = OpTypeInt 32 1\n"
	    "%v2int = OpTypeVector %int 2\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%image = OpTypeImage %float 2D 0 0 0 2 Rgba8\n"
	    "%ptrOutputV4 = OpTypePointer Output %v4float\n"
	    "%ptrUniformConstantImage = OpTypePointer UniformConstant %image\n"
	    "%outColor = OpVariable %ptrOutputV4 Output\n"
	    "%storageImage = OpVariable %ptrUniformConstantImage UniformConstant\n"
	    "%int_0 = OpConstant %int 0\n"
	    "%coord = OpConstantComposite %v2int %int_0 %int_0\n"
	    "%float_1 = OpConstant %float 1\n"
	    "%float_0 = OpConstant %float 0\n"
	    "%red = OpConstantComposite %v4float %float_1 %float_0 %float_0 %float_1\n"
	    "%main = OpFunction %void None %func\n"
	    "%entry = OpLabel\n"
	    "%imageHandle = OpLoad %image %storageImage\n"
	    "OpImageWrite %imageHandle %coord %red\n"
	    "OpStore %outColor %red\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary discardFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
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
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "OpKill\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary derivativeFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %inTexCoord %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpName %main \"main\"\n"
	    "OpName %dx \"dx\"\n"
	    "OpName %inTexCoord \"inTexCoord\"\n"
	    "OpName %outColor \"outColor\"\n"
	    "OpDecorate %inTexCoord Location 0\n"
	    "OpDecorate %outColor Location 0\n"
	    "%void = OpTypeVoid\n"
	    "%3 = OpTypeFunction %void\n"
	    "%float = OpTypeFloat 32\n"
	    "%_ptr_Function_float = OpTypePointer Function %float\n"
	    "%v2float = OpTypeVector %float 2\n"
	    "%_ptr_Input_v2float = OpTypePointer Input %v2float\n"
	    "%inTexCoord = OpVariable %_ptr_Input_v2float Input\n"
	    "%uint = OpTypeInt 32 0\n"
	    "%uint_0 = OpConstant %uint 0\n"
	    "%_ptr_Input_float = OpTypePointer Input %float\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
	    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
	    "%float_0 = OpConstant %float 0\n"
	    "%float_1 = OpConstant %float 1\n"
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "%dx = OpVariable %_ptr_Function_float Function\n"
	    "%15 = OpAccessChain %_ptr_Input_float %inTexCoord %uint_0\n"
	    "%16 = OpLoad %float %15\n"
	    "%17 = OpDPdx %float %16\n"
	    "OpStore %dx %17\n"
	    "%21 = OpLoad %float %dx\n"
	    "%24 = OpCompositeConstruct %v4float %21 %float_0 %float_0 %float_1\n"
	    "OpStore %outColor %24\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary imageQueryFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "OpCapability ImageQuery\n"
	    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpSourceExtension \"GL_EXT_samplerless_texture_functions\"\n"
	    "OpName %main \"main\"\n"
	    "OpName %size \"size\"\n"
	    "OpName %tex \"tex\"\n"
	    "OpName %outColor \"outColor\"\n"
	    "OpDecorate %tex DescriptorSet 0\n"
	    "OpDecorate %tex Binding 0\n"
	    "OpDecorate %outColor Location 0\n"
	    "%void = OpTypeVoid\n"
	    "%3 = OpTypeFunction %void\n"
	    "%int = OpTypeInt 32 1\n"
	    "%v2int = OpTypeVector %int 2\n"
	    "%_ptr_Function_v2int = OpTypePointer Function %v2int\n"
	    "%float = OpTypeFloat 32\n"
	    "%11 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
	    "%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11\n"
	    "%tex = OpVariable %_ptr_UniformConstant_11 UniformConstant\n"
	    "%int_0 = OpConstant %int 0\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
	    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
	    "%uint = OpTypeInt 32 0\n"
	    "%uint_0 = OpConstant %uint 0\n"
	    "%_ptr_Function_int = OpTypePointer Function %int\n"
	    "%uint_1 = OpConstant %uint 1\n"
	    "%float_0 = OpConstant %float 0\n"
	    "%float_1 = OpConstant %float 1\n"
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "%size = OpVariable %_ptr_Function_v2int Function\n"
	    "%14 = OpLoad %11 %tex\n"
	    "%16 = OpImageQuerySizeLod %v2int %14 %int_0\n"
	    "OpStore %size %16\n"
	    "%23 = OpAccessChain %_ptr_Function_int %size %uint_0\n"
	    "%24 = OpLoad %int %23\n"
	    "%25 = OpConvertSToF %float %24\n"
	    "%27 = OpAccessChain %_ptr_Function_int %size %uint_1\n"
	    "%28 = OpLoad %int %27\n"
	    "%29 = OpConvertSToF %float %28\n"
	    "%32 = OpCompositeConstruct %v4float %25 %29 %float_0 %float_1\n"
	    "OpStore %outColor %32\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary imageFetchFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpSourceExtension \"GL_EXT_samplerless_texture_functions\"\n"
	    "OpName %main \"main\"\n"
	    "OpName %c \"c\"\n"
	    "OpName %tex \"tex\"\n"
	    "OpName %outColor \"outColor\"\n"
	    "OpDecorate %tex DescriptorSet 0\n"
	    "OpDecorate %tex Binding 0\n"
	    "OpDecorate %outColor Location 0\n"
	    "%void = OpTypeVoid\n"
	    "%3 = OpTypeFunction %void\n"
	    "%float = OpTypeFloat 32\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%_ptr_Function_v4float = OpTypePointer Function %v4float\n"
	    "%10 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
	    "%_ptr_UniformConstant_10 = OpTypePointer UniformConstant %10\n"
	    "%tex = OpVariable %_ptr_UniformConstant_10 UniformConstant\n"
	    "%int = OpTypeInt 32 1\n"
	    "%v2int = OpTypeVector %int 2\n"
	    "%int_0 = OpConstant %int 0\n"
	    "%17 = OpConstantComposite %v2int %int_0 %int_0\n"
	    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
	    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "%c = OpVariable %_ptr_Function_v4float Function\n"
	    "%13 = OpLoad %10 %tex\n"
	    "%18 = OpImageFetch %v4float %13 %17 Lod %int_0\n"
	    "OpStore %c %18\n"
	    "%21 = OpLoad %v4float %c\n"
	    "OpStore %outColor %21\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary atomicsFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpName %main \"main\"\n"
	    "OpName %old \"old\"\n"
	    "OpName %CounterBlock \"CounterBlock\"\n"
	    "OpMemberName %CounterBlock 0 \"counter\"\n"
	    "OpName %_ \"\"\n"
	    "OpName %outColor \"outColor\"\n"
	    "OpMemberDecorate %CounterBlock 0 Offset 0\n"
	    "OpDecorate %CounterBlock BufferBlock\n"
	    "OpDecorate %_ DescriptorSet 0\n"
	    "OpDecorate %_ Binding 0\n"
	    "OpDecorate %outColor Location 0\n"
	    "%void = OpTypeVoid\n"
	    "%3 = OpTypeFunction %void\n"
	    "%uint = OpTypeInt 32 0\n"
	    "%_ptr_Function_uint = OpTypePointer Function %uint\n"
	    "%CounterBlock = OpTypeStruct %uint\n"
	    "%_ptr_Uniform_CounterBlock = OpTypePointer Uniform %CounterBlock\n"
	    "%_ = OpVariable %_ptr_Uniform_CounterBlock Uniform\n"
	    "%int = OpTypeInt 32 1\n"
	    "%int_0 = OpConstant %int 0\n"
	    "%_ptr_Uniform_uint = OpTypePointer Uniform %uint\n"
	    "%uint_1 = OpConstant %uint 1\n"
	    "%uint_0 = OpConstant %uint 0\n"
	    "%float = OpTypeFloat 32\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
	    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
	    "%float_0 = OpConstant %float 0\n"
	    "%float_1 = OpConstant %float 1\n"
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "%old = OpVariable %_ptr_Function_uint Function\n"
	    "%15 = OpAccessChain %_ptr_Uniform_uint %_ %int_0\n"
	    "%18 = OpAtomicIAdd %uint %15 %uint_1 %uint_0 %uint_1\n"
	    "OpStore %old %18\n"
	    "%23 = OpLoad %uint %old\n"
	    "%24 = OpConvertUToF %float %23\n"
	    "%27 = OpCompositeConstruct %v4float %24 %float_0 %float_0 %float_1\n"
	    "OpStore %outColor %27\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary subgroupFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "OpCapability GroupNonUniform\n"
	    "%1 = OpExtInstImport \"GLSL.std.450\"\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpSourceExtension \"GL_KHR_shader_subgroup_basic\"\n"
	    "OpName %main \"main\"\n"
	    "OpName %e \"e\"\n"
	    "OpName %outColor \"outColor\"\n"
	    "OpDecorate %outColor Location 0\n"
	    "%void = OpTypeVoid\n"
	    "%3 = OpTypeFunction %void\n"
	    "%bool = OpTypeBool\n"
	    "%_ptr_Function_bool = OpTypePointer Function %bool\n"
	    "%uint = OpTypeInt 32 0\n"
	    "%uint_3 = OpConstant %uint 3\n"
	    "%float = OpTypeFloat 32\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
	    "%outColor = OpVariable %_ptr_Output_v4float Output\n"
	    "%float_1 = OpConstant %float 1\n"
	    "%float_0 = OpConstant %float 0\n"
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "%e = OpVariable %_ptr_Function_bool Function\n"
	    "%11 = OpGroupNonUniformElect %bool %uint_3\n"
	    "OpStore %e %11\n"
	    "%16 = OpLoad %bool %e\n"
	    "%19 = OpSelect %float %16 %float_1 %float_0\n"
	    "%20 = OpCompositeConstruct %v4float %19 %float_0 %float_0 %float_1\n"
	    "OpStore %outColor %20\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly, SPV_ENV_UNIVERSAL_1_3);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary combinedImageSamplerDescriptorArrayIndexOneFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %inTexCoord %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpName %main \"main\"\n"
	    "OpDecorate %inTexCoord Location 0\n"
	    "OpDecorate %outColor Location 0\n"
	    "OpDecorate %texArray DescriptorSet 0\n"
	    "OpDecorate %texArray Binding 1\n"
	    "%void = OpTypeVoid\n"
	    "%func = OpTypeFunction %void\n"
	    "%float = OpTypeFloat 32\n"
	    "%v2float = OpTypeVector %float 2\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%uint = OpTypeInt 32 0\n"
	    "%uint_2 = OpConstant %uint 2\n"
	    "%uint_1 = OpConstant %uint 1\n"
	    "%image = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
	    "%sampledImage = OpTypeSampledImage %image\n"
	    "%sampledArray = OpTypeArray %sampledImage %uint_2\n"
	    "%ptrInputV2 = OpTypePointer Input %v2float\n"
	    "%ptrOutputV4 = OpTypePointer Output %v4float\n"
	    "%ptrUniformConstantSampledArray = OpTypePointer UniformConstant %sampledArray\n"
	    "%ptrUniformConstantSampledImage = OpTypePointer UniformConstant %sampledImage\n"
	    "%inTexCoord = OpVariable %ptrInputV2 Input\n"
	    "%outColor = OpVariable %ptrOutputV4 Output\n"
	    "%texArray = OpVariable %ptrUniformConstantSampledArray UniformConstant\n"
	    "%main = OpFunction %void None %func\n"
	    "%entry = OpLabel\n"
	    "%coord = OpLoad %v2float %inTexCoord\n"
	    "%texPtr = OpAccessChain %ptrUniformConstantSampledImage %texArray %uint_1\n"
	    "%sampler = OpLoad %sampledImage %texPtr\n"
	    "%color = OpImageSampleImplicitLod %v4float %sampler %coord\n"
	    "OpStore %outColor %color\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::SpirvBinary uniformBufferGuardFragmentBinary()
{
	static constexpr const char kFragmentAssembly[] =
	    "OpCapability Shader\n"
	    "OpMemoryModel Logical GLSL450\n"
	    "OpEntryPoint Fragment %main \"main\" %outColor\n"
	    "OpExecutionMode %main OriginUpperLeft\n"
	    "OpSource GLSL 450\n"
	    "OpName %main \"main\"\n"
	    "OpDecorate %outColor Location 0\n"
	    "OpDecorate %guardBlock Block\n"
	    "OpMemberDecorate %guardBlock 0 Offset 0\n"
	    "OpDecorate %guard DescriptorSet 0\n"
	    "OpDecorate %guard Binding 0\n"
	    "%void = OpTypeVoid\n"
	    "%func = OpTypeFunction %void\n"
	    "%float = OpTypeFloat 32\n"
	    "%v4float = OpTypeVector %float 4\n"
	    "%guardBlock = OpTypeStruct %float\n"
	    "%ptrUniformGuardBlock = OpTypePointer Uniform %guardBlock\n"
	    "%ptrUniformFloat = OpTypePointer Uniform %float\n"
	    "%ptrOutputV4 = OpTypePointer Output %v4float\n"
	    "%guard = OpVariable %ptrUniformGuardBlock Uniform\n"
	    "%outColor = OpVariable %ptrOutputV4 Output\n"
	    "%int = OpTypeInt 32 1\n"
	    "%int_0 = OpConstant %int 0\n"
	    "%float_1 = OpConstant %float 1\n"
	    "%float_0 = OpConstant %float 0\n"
	    "%red = OpConstantComposite %v4float %float_1 %float_0 %float_0 %float_1\n"
	    "%main = OpFunction %void None %func\n"
	    "%entry = OpLabel\n"
	    "%guardPtr = OpAccessChain %ptrUniformFloat %guard %int_0\n"
	    "%guardValue = OpLoad %float %guardPtr\n"
	    "OpStore %outColor %red\n"
	    "OpReturn\n"
	    "OpFunctionEnd\n";

	auto spirv = compileSpirv(kFragmentAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

sw::ShaderCompilerAnalysisContext defaultContext()
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

		if(binding == 1)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindingInfo->descriptorCount = 2;
			return true;
		}
		if(binding == 2)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindingInfo->descriptorCount = 1;
			return true;
		}
		return false;
	};
	return context;
}

sw::ShaderCompilerAnalysisContext uniformBufferContext()
{
	sw::ShaderCompilerAnalysisContext context = defaultContext();
	context.queryDescriptorBindingInfo = [](const void *, uint32_t descriptorSet, uint32_t binding, sw::ShaderDescriptorBindingInfo *bindingInfo) {
		if(bindingInfo == nullptr || descriptorSet != 0)
		{
			return false;
		}

		if(binding == 0)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			bindingInfo->descriptorCount = 1;
			return true;
		}
		if(binding == 1)
		{
			bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindingInfo->descriptorCount = 2;
			return true;
		}
		return false;
	};
	return context;
}

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

sw::ShaderCompilerAnalysisContext imageOnlyContext()
{
	sw::ShaderCompilerAnalysisContext context = {};
	context.descriptorSetCount = 1;
	context.dynamicOffsetCount = 0;
	context.pushConstantSize = vk::MAX_PUSH_CONSTANT_SIZE;
	context.queryDescriptorBindingInfo = [](const void *, uint32_t descriptorSet, uint32_t binding, sw::ShaderDescriptorBindingInfo *bindingInfo) {
		if(bindingInfo == nullptr || descriptorSet != 0 || binding != 0)
		{
			return false;
		}
		bindingInfo->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		bindingInfo->descriptorCount = 1;
		return true;
	};
	return context;
}

}  // namespace

TEST(SpirvToCompilerAnalysis, ClassifiesCombinedImageSamplerFragment)
{
	sw::SpirvBinary spirv = combinedImageSamplerFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, defaultContext());

	EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
	EXPECT_EQ(result.fragmentFeatureMask, 0u);
	EXPECT_EQ(result.unsupportedReasonMask, 0u);
}

TEST(SpirvToCompilerAnalysis, ClassifiesCombinedImageSamplerFromAssemblyText)
{
	const std::string assembly =
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

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShaderAssembly("main", assembly, defaultContext());

	EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
	EXPECT_EQ(result.fragmentFeatureMask, 0u);
}

TEST(SpirvToCompilerAnalysis, UnifiedInputAcceptsBinaryAndAssembly)
{
	const std::string assembly =
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

	const auto words = compileSpirv(assembly.c_str());
	const sw::SpirvBinary binary(words.data(), static_cast<uint32_t>(words.size()));

	const sw::ShaderCompilerAnalysisResult fromBinary =
	    sw::analyzeGraphicsFragmentShader(sw::ShaderModuleInput::fromBinary("main", binary), defaultContext());
	const sw::ShaderCompilerAnalysisResult fromAssembly =
	    sw::analyzeGraphicsFragmentShader(sw::ShaderModuleInput::fromAssembly("main", assembly), defaultContext());

	EXPECT_EQ(fromBinary.texturePlan.resourceKind, fromAssembly.texturePlan.resourceKind);
	EXPECT_EQ(fromBinary.fragmentFeatureMask, fromAssembly.fragmentFeatureMask);
	EXPECT_EQ(fromBinary.unsupportedReasonMask, fromAssembly.unsupportedReasonMask);
}

TEST(SpirvToCompilerAnalysis, MarksStorageImageWriteAsUnsupported)
{
	sw::SpirvBinary spirv = storageImageWriteFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, defaultContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::StorageImageReadWrite),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::StorageImageReadWrite),
	          0u);
}

TEST(SpirvToCompilerAnalysis, ClassifiesSeparateImageSamplerFragment)
{
	sw::SpirvBinary spirv = separateImageSamplerFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, separateSamplerContext());

	EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::SeparateImageSampler);
	EXPECT_EQ(result.fragmentFeatureMask, 0u);
}

TEST(SpirvToCompilerAnalysis, TracksCombinedImageSamplerDescriptorArrayIndexOne)
{
	sw::SpirvBinary spirv = combinedImageSamplerDescriptorArrayIndexOneFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, defaultContext());

	EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
	EXPECT_EQ(result.texturePlan.imageArrayElement, 1u);
	EXPECT_EQ(result.texturePlan.samplerArrayElement, 1u);
}

TEST(SpirvToCompilerAnalysis, SupportsCombinedImageSamplerBootstrapForSamplePassthrough)
{
    static constexpr const char kAssembly[] =
        "OpCapability Shader\n"
        "%1 = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint Fragment %main \"main\" %inTexCoord %outColor\n"
        "OpExecutionMode %main OriginUpperLeft\n"
        "OpSource GLSL 450\n"
        "OpName %main \"main\"\n"
        "OpName %sampledColor \"sampledColor\"\n"
        "OpName %tex \"tex\"\n"
        "OpName %inTexCoord \"inTexCoord\"\n"
        "OpName %outColor \"outColor\"\n"
        "OpDecorate %tex DescriptorSet 0\n"
        "OpDecorate %tex Binding 1\n"
        "OpDecorate %inTexCoord Location 0\n"
        "OpDecorate %outColor Location 0\n"
        "%void = OpTypeVoid\n"
        "%3 = OpTypeFunction %void\n"
        "%float = OpTypeFloat 32\n"
        "%v4float = OpTypeVector %float 4\n"
        "%_ptr_Function_v4float = OpTypePointer Function %v4float\n"
        "%10 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
        "%11 = OpTypeSampledImage %10\n"
        "%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11\n"
        "%tex = OpVariable %_ptr_UniformConstant_11 UniformConstant\n"
        "%v2float = OpTypeVector %float 2\n"
        "%_ptr_Input_v2float = OpTypePointer Input %v2float\n"
        "%inTexCoord = OpVariable %_ptr_Input_v2float Input\n"
        "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
        "%outColor = OpVariable %_ptr_Output_v4float Output\n"
        "%main = OpFunction %void None %3\n"
        "%5 = OpLabel\n"
        "%sampledColor = OpVariable %_ptr_Function_v4float Function\n"
        "%14 = OpLoad %11 %tex\n"
        "%18 = OpLoad %v2float %inTexCoord\n"
        "%19 = OpImageSampleImplicitLod %v4float %14 %18\n"
        "OpStore %sampledColor %19\n"
        "%22 = OpLoad %v4float %sampledColor\n"
        "OpStore %outColor %22\n"
        "OpReturn\n"
        "OpFunctionEnd\n";

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
    EXPECT_TRUE(result.texturePlan.bootstrapSupported);
}

TEST(SpirvToCompilerAnalysis, RejectsCombinedImageSamplerBootstrapForPostProcessedSample)
{
    static constexpr const char kAssembly[] =
        "OpCapability Shader\n"
        "%1 = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint Fragment %main \"main\" %outColor %inTexCoord\n"
        "OpExecutionMode %main OriginUpperLeft\n"
        "OpSource GLSL 450\n"
        "OpName %main \"main\"\n"
        "OpName %outColor \"outColor\"\n"
        "OpName %tex \"tex\"\n"
        "OpName %inTexCoord \"inTexCoord\"\n"
        "OpDecorate %outColor Location 0\n"
        "OpDecorate %tex DescriptorSet 0\n"
        "OpDecorate %tex Binding 1\n"
        "OpDecorate %inTexCoord Location 0\n"
        "%void = OpTypeVoid\n"
        "%3 = OpTypeFunction %void\n"
        "%float = OpTypeFloat 32\n"
        "%v4float = OpTypeVector %float 4\n"
        "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
        "%outColor = OpVariable %_ptr_Output_v4float Output\n"
        "%10 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
        "%11 = OpTypeSampledImage %10\n"
        "%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11\n"
        "%tex = OpVariable %_ptr_UniformConstant_11 UniformConstant\n"
        "%v2float = OpTypeVector %float 2\n"
        "%_ptr_Input_v2float = OpTypePointer Input %v2float\n"
        "%inTexCoord = OpVariable %_ptr_Input_v2float Input\n"
        "%float_0_5 = OpConstant %float 0.5\n"
        "%main = OpFunction %void None %3\n"
        "%5 = OpLabel\n"
        "%14 = OpLoad %11 %tex\n"
        "%18 = OpLoad %v2float %inTexCoord\n"
        "%19 = OpImageSampleImplicitLod %v4float %14 %18\n"
        "%21 = OpVectorTimesScalar %v4float %19 %float_0_5\n"
        "OpStore %outColor %21\n"
        "OpReturn\n"
        "OpFunctionEnd\n";

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
    EXPECT_FALSE(result.texturePlan.bootstrapSupported);
    EXPECT_NE(result.unsupportedReasonMask &
                  static_cast<uint32_t>(sw::ShaderUnsupportedReason::TextureSamplingUnsupported),
              0u);
}

TEST(SpirvToCompilerAnalysis, RejectsCombinedImageSamplerBootstrapWhenLocationZeroIsNotVec2)
{
    static constexpr const char kAssembly[] =
        "OpCapability Shader\n"
        "%1 = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint Fragment %main \"main\" %outColor %inTexCoord3\n"
        "OpExecutionMode %main OriginUpperLeft\n"
        "OpSource GLSL 450\n"
        "OpName %main \"main\"\n"
        "OpName %outColor \"outColor\"\n"
        "OpName %tex \"tex\"\n"
        "OpName %inTexCoord3 \"inTexCoord3\"\n"
        "OpDecorate %outColor Location 0\n"
        "OpDecorate %tex DescriptorSet 0\n"
        "OpDecorate %tex Binding 1\n"
        "OpDecorate %inTexCoord3 Location 0\n"
        "%void = OpTypeVoid\n"
        "%3 = OpTypeFunction %void\n"
        "%float = OpTypeFloat 32\n"
        "%v4float = OpTypeVector %float 4\n"
        "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
        "%outColor = OpVariable %_ptr_Output_v4float Output\n"
        "%10 = OpTypeImage %float 2D 0 0 0 1 Unknown\n"
        "%11 = OpTypeSampledImage %10\n"
        "%_ptr_UniformConstant_11 = OpTypePointer UniformConstant %11\n"
        "%tex = OpVariable %_ptr_UniformConstant_11 UniformConstant\n"
        "%v3float = OpTypeVector %float 3\n"
        "%_ptr_Input_v3float = OpTypePointer Input %v3float\n"
        "%inTexCoord3 = OpVariable %_ptr_Input_v3float Input\n"
        "%v2float = OpTypeVector %float 2\n"
        "%main = OpFunction %void None %3\n"
        "%5 = OpLabel\n"
        "%14 = OpLoad %11 %tex\n"
        "%19 = OpLoad %v3float %inTexCoord3\n"
        "%20 = OpVectorShuffle %v2float %19 %19 0 1\n"
        "%21 = OpImageSampleImplicitLod %v4float %14 %20\n"
        "OpStore %outColor %21\n"
        "OpReturn\n"
        "OpFunctionEnd\n";

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.texturePlan.resourceKind, sw::ShaderTextureResourceKind::CombinedImageSampler);
    EXPECT_FALSE(result.texturePlan.bootstrapSupported);
}

TEST(SpirvToCompilerAnalysis, MarksUniformBufferAsUnsupportedBufferDescriptor)
{
	sw::SpirvBinary spirv = uniformBufferGuardFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, uniformBufferContext());

	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::BufferDescriptorsPresent),
	          0u);
}

TEST(SpirvToCompilerAnalysis, MarksDiscardAsUnsupported)
{
	sw::SpirvBinary spirv = discardFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, defaultContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::Discard),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::DiscardUnsupported),
	          0u);
}

TEST(SpirvToCompilerAnalysis, MarksDiscardAsUnsupportedFromAssemblyText)
{
	const std::string assembly =
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
	    "%main = OpFunction %void None %3\n"
	    "%5 = OpLabel\n"
	    "OpKill\n"
	    "OpFunctionEnd\n";

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShaderAssembly("main", assembly, defaultContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::Discard),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::DiscardUnsupported),
	          0u);
}

TEST(SpirvToCompilerAnalysis, RecognizesFragCoordDiscardLeftConstantColorStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::FragCoordDiscardLeftConstantColor);
}

TEST(SpirvToCompilerAnalysis, RecognizesFragCoordQuadrantsStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::FragCoordQuadrants);
}

TEST(SpirvToCompilerAnalysis, RecognizesFrontFacingStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

	EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::FrontFacingBinaryColors);
}

TEST(SpirvToCompilerAnalysis, RecognizesPointCoordGradientStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::PointCoordGradient);
}

TEST(SpirvToCompilerAnalysis, RecognizesFlatInterpolatedColorStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::FlatInterpolatedColor);
}

TEST(SpirvToCompilerAnalysis, RecognizesInterpolatedColorStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::InterpolatedColor);
}

TEST(SpirvToCompilerAnalysis, RecognizesInterpolatedColorFragDepthStaticTemplate)
{
    static constexpr const char kAssembly[] =
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

    sw::ShaderCompilerAnalysisResult result =
        sw::analyzeGraphicsFragmentShaderAssembly("main", kAssembly, defaultContext());

    EXPECT_EQ(result.staticFragmentKind, sw::ShaderStaticFragmentKind::InterpolatedColorBlueNearFragDepth);
}

TEST(SpirvToCompilerAnalysis, MarksDerivativesAsUnsupported)
{
	sw::SpirvBinary spirv = derivativeFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, defaultContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::Derivatives),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::Derivatives),
	          0u);
}

TEST(SpirvToCompilerAnalysis, MarksImageQueryAsUnsupported)
{
	sw::SpirvBinary spirv = imageQueryFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, imageOnlyContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::ImageQueryOrFetch),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::ImageQueryOrFetch),
	          0u);
}

TEST(SpirvToCompilerAnalysis, MarksImageFetchAsUnsupported)
{
	sw::SpirvBinary spirv = imageFetchFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, imageOnlyContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::ImageQueryOrFetch),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::ImageQueryOrFetch),
	          0u);
}

TEST(SpirvToCompilerAnalysis, MarksAtomicsAsUnsupported)
{
	sw::SpirvBinary spirv = atomicsFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, uniformBufferContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::Atomics),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::Atomics),
	          0u);
}

TEST(SpirvToCompilerAnalysis, MarksSubgroupAsUnsupported)
{
	sw::SpirvBinary spirv = subgroupFragmentBinary();

	sw::ShaderCompilerAnalysisResult result =
	    sw::analyzeGraphicsFragmentShader("main", spirv, defaultContext());

	EXPECT_NE(result.fragmentFeatureMask &
	              static_cast<uint32_t>(sw::ShaderFragmentFeature::Subgroup),
	          0u);
	EXPECT_NE(result.unsupportedReasonMask &
	              static_cast<uint32_t>(sw::ShaderUnsupportedReason::Subgroup),
	          0u);
}
