#include "Backend/BackendFactory.hpp"
#include "Backend/ComputeExecutable.hpp"
#include "Pipeline/SemanticIRBuilder.hpp"

#include <gtest/gtest.h>
#include <spirv/unified1/spirv.hpp>

TEST(BackendSmoke, ComputePathCanCompile)
{
	sw::ParsedSpirvInfo parsed = { VK_SHADER_STAGE_COMPUTE_BIT, "main" };
	const uint32_t words[] = { spv::MagicNumber, 0x00010300, 0, 4, 0 };
	sw::SpirvBinary spirv(words, sizeof(words) / sizeof(words[0]));
	auto executable = backend::ComputeExecutable::create(parsed, spirv);
	ASSERT_NE(executable, nullptr);
	EXPECT_TRUE(executable->valid());
}

TEST(BackendSmoke, DefaultBackendSelectionMatchesBuildFlags)
{
#if SWIFTSHADER_ENABLE_GPU_BACKEND
	EXPECT_EQ(backend::defaultBackendKind(), backend::BackendKind::GPU);
#else
	EXPECT_EQ(backend::defaultBackendKind(), backend::BackendKind::CPU);
#endif
}
