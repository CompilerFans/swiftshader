#include "Backend/BackendFactory.hpp"
#include "Backend/ComputeExecutable.hpp"
#include "Pipeline/SemanticIRBuilder.hpp"

#include <gtest/gtest.h>

TEST(BackendSmoke, ComputePathCanCompile)
{
	sw::ParsedSpirvInfo parsed = { VK_SHADER_STAGE_COMPUTE_BIT, "main" };
	auto executable = backend::ComputeExecutable::create(parsed);
	ASSERT_NE(executable, nullptr);
	EXPECT_TRUE(executable->valid());
}

TEST(BackendSmoke, GraphicsPathStillFallsBackToCpu)
{
	EXPECT_EQ(backend::defaultBackendKind(), backend::BackendKind::CPU);
}
