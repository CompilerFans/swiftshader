#include "Backend/GraphicsExecutable.hpp"
#include "Pipeline/SemanticIRBuilder.hpp"

#include <gtest/gtest.h>

namespace {

std::shared_ptr<sw::SemanticIRModule> buildModule(const sw::ParsedSpirvInfo &parsed)
{
	sw::SemanticIRBuilder builder;
	return builder.build(parsed);
}

}  // namespace

TEST(GraphicsExecutable, PreservesVertexLoweringAndOptionalFragmentStage)
{
	sw::ParsedSpirvInfo vertexParsed = { VK_SHADER_STAGE_VERTEX_BIT, "vs_main" };
	vertexParsed.vertexLowering.usesPositionAttribute = true;
	vertexParsed.vertexLowering.positionAttributeLocation = 0;
	vertexParsed.vertexLowering.positionBinding = 2;
	vertexParsed.vertexLowering.positionInputComponentCount = 3;
	vertexParsed.vertexLowering.vertexStride = 16;
	vertexParsed.vertexLowering.positionOffset = 4;
	vertexParsed.vertexLowering.usesVertexIndex = true;
	vertexParsed.vertexLowering.usesInstanceIndex = true;

	sw::ParsedSpirvInfo fragmentParsed = { VK_SHADER_STAGE_FRAGMENT_BIT, "fs_main" };

	auto executable = backend::GraphicsExecutable::create(buildModule(vertexParsed),
	                                                      buildModule(fragmentParsed));
	ASSERT_NE(executable, nullptr);
	EXPECT_TRUE(executable->valid());
	EXPECT_EQ(executable->vertexEntryPoint(), "vs_main");
	EXPECT_TRUE(executable->hasFragmentStage());
	EXPECT_EQ(executable->fragmentEntryPoint(), "fs_main");
	EXPECT_TRUE(executable->vertexLowering().usesPositionAttribute);
	EXPECT_EQ(executable->vertexLowering().positionBinding, 2u);
	EXPECT_EQ(executable->vertexLowering().vertexStride, 16u);
	EXPECT_EQ(executable->vertexLowering().positionOffset, 4u);
	EXPECT_TRUE(executable->vertexLowering().usesVertexIndex);
	EXPECT_TRUE(executable->vertexLowering().usesInstanceIndex);
}

TEST(GraphicsExecutable, AllowsVertexOnlyPipelines)
{
	sw::ParsedSpirvInfo vertexParsed = { VK_SHADER_STAGE_VERTEX_BIT, "main" };
	backend::GraphicsExecutableCreateInfo createInfo = {};
	createInfo.vertexModule = buildModule(vertexParsed);
	auto executable = backend::GraphicsExecutable::create(createInfo);
	ASSERT_NE(executable, nullptr);
	EXPECT_TRUE(executable->valid());
	EXPECT_FALSE(executable->hasFragmentStage());
	EXPECT_EQ(executable->fragmentEntryPoint(), "");
	EXPECT_FALSE(executable->hasBootstrapFragmentConfig());
	EXPECT_FALSE(executable->hasBootstrapTextureBinding());
	EXPECT_FLOAT_EQ(executable->bootstrapPointSize(), 64.0f);
}

TEST(GraphicsExecutable, RejectsMissingVertexOrWrongStageCombinations)
{
	sw::ParsedSpirvInfo vertexParsed = { VK_SHADER_STAGE_VERTEX_BIT, "vs_main" };
	sw::ParsedSpirvInfo fragmentParsed = { VK_SHADER_STAGE_FRAGMENT_BIT, "fs_main" };

	EXPECT_EQ(backend::GraphicsExecutable::create(nullptr, buildModule(fragmentParsed)), nullptr);
	EXPECT_EQ(backend::GraphicsExecutable::create(buildModule(fragmentParsed), nullptr), nullptr);
	EXPECT_EQ(backend::GraphicsExecutable::create(buildModule(vertexParsed), buildModule(vertexParsed)), nullptr);
}
