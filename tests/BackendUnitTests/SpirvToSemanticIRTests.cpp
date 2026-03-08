#include "Pipeline/SemanticIRBuilder.hpp"
#include "Pipeline/SpirvBinary.hpp"

#include <gtest/gtest.h>
#include "spirv-tools/libspirv.hpp"

namespace {

std::vector<uint32_t> compileSpirv(const char *assembly)
{
	spvtools::SpirvTools core(SPV_ENV_VULKAN_1_0);
	core.SetMessageConsumer([](spv_message_level_t, const char *, const spv_position_t &position, const char *message) {
		FAIL() << position.line << ":" << position.column << ": " << message;
	});

	std::vector<uint32_t> spirv;
	EXPECT_TRUE(core.Assemble(assembly, &spirv));
	EXPECT_TRUE(core.Validate(spirv));
	return spirv;
}

sw::SpirvBinary minimalVertexShaderBinary()
{
	static constexpr const char kVertexAssembly[] =
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

	auto spirv = compileSpirv(kVertexAssembly);
	return sw::SpirvBinary(spirv.data(), static_cast<uint32_t>(spirv.size()));
}

}  // namespace

TEST(SpirvToSemanticIR, BuildSemanticIRForParsedShaderInfo)
{
	sw::ParsedSpirvInfo parsed = { VK_SHADER_STAGE_COMPUTE_BIT, "main" };

	sw::SemanticIRBuilder builder;
	auto module = builder.build(parsed);

	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->stage(), VK_SHADER_STAGE_COMPUTE_BIT);
	EXPECT_EQ(module->entryPoint(), "main");
}

TEST(SpirvToSemanticIR, PreservesMinimalVertexLoweringInfo)
{
	sw::ParsedSpirvInfo parsed = { VK_SHADER_STAGE_VERTEX_BIT, "main" };
	parsed.vertexLowering.usesPositionAttribute = true;
	parsed.vertexLowering.positionAttributeLocation = 0;
	parsed.vertexLowering.positionBinding = 0;
	parsed.vertexLowering.vertexStride = 12;
	parsed.vertexLowering.positionOffset = 0;
	parsed.vertexLowering.usesVertexIndex = true;
	parsed.vertexLowering.usesInstanceIndex = true;
	parsed.vertexLowering.constantOffsetX = 0.25f;

	sw::SemanticIRBuilder builder;
	auto module = builder.build(parsed);

	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->stage(), VK_SHADER_STAGE_VERTEX_BIT);
	EXPECT_TRUE(module->vertexLowering().usesPositionAttribute);
	EXPECT_EQ(module->vertexLowering().positionAttributeLocation, 0u);
	EXPECT_EQ(module->vertexLowering().positionBinding, 0u);
	EXPECT_EQ(module->vertexLowering().vertexStride, 12u);
	EXPECT_EQ(module->vertexLowering().positionOffset, 0u);
	EXPECT_TRUE(module->vertexLowering().usesVertexIndex);
	EXPECT_TRUE(module->vertexLowering().usesInstanceIndex);
	EXPECT_FLOAT_EQ(module->vertexLowering().constantOffsetX, 0.25f);
}

TEST(SpirvToSemanticIR, ExtractsMinimalVertexLoweringInfoFromSpirvBinary)
{
	sw::SpirvBinary spirv = minimalVertexShaderBinary();

	sw::SemanticIRBuilder builder;
	auto module = builder.build(VK_SHADER_STAGE_VERTEX_BIT, "main", spirv);

	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->stage(), VK_SHADER_STAGE_VERTEX_BIT);
	EXPECT_TRUE(module->vertexLowering().usesPositionAttribute);
	EXPECT_EQ(module->vertexLowering().positionAttributeLocation, 0u);
	EXPECT_TRUE(module->vertexLowering().usesVertexIndex);
	EXPECT_TRUE(module->vertexLowering().usesInstanceIndex);
}
