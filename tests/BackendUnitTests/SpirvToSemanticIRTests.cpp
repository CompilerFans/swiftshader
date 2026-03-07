#include "Pipeline/SemanticIRBuilder.hpp"

#include <gtest/gtest.h>

TEST(SpirvToSemanticIR, BuildSemanticIRForParsedShaderInfo)
{
	sw::ParsedSpirvInfo parsed = { VK_SHADER_STAGE_COMPUTE_BIT, "main" };

	sw::SemanticIRBuilder builder;
	auto module = builder.build(parsed);

	ASSERT_NE(module, nullptr);
	EXPECT_EQ(module->stage(), VK_SHADER_STAGE_COMPUTE_BIT);
	EXPECT_EQ(module->entryPoint(), "main");
}
