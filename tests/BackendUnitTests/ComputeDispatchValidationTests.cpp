#include "Backend/ComputeExecutable.hpp"
#include "Backend/FakeRuntimeAPI.hpp"
#include "Pipeline/SemanticIRBuilder.hpp"

#include <gtest/gtest.h>

TEST(ComputeDispatchValidation, FakeRuntimeCapturesLaunchAndBindings)
{
	sw::ParsedSpirvInfo parsed = { VK_SHADER_STAGE_COMPUTE_BIT, "main" };
	auto executable = backend::ComputeExecutable::create(parsed);

	ASSERT_NE(executable, nullptr);
	ASSERT_TRUE(executable->valid());

	backend::FakeRuntimeAPI runtime;
	backend::ComputeDispatchInfo dispatch = {};
	dispatch.groupCountX = 4;
	dispatch.groupCountY = 2;
	dispatch.groupCountZ = 1;
	dispatch.bindingCount = 2;
	dispatch.argumentWords = 8;

	executable->dispatch(runtime, dispatch);

	EXPECT_FALSE(runtime.lastModuleSource().empty());
	EXPECT_TRUE(runtime.lastLaunch().module.valid());
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 4u);
	EXPECT_EQ(runtime.lastLaunch().groupCountY, 2u);
	EXPECT_EQ(runtime.lastLaunch().groupCountZ, 1u);
	EXPECT_EQ(runtime.lastLaunch().bindingCount, 2u);
	EXPECT_EQ(runtime.lastLaunch().argumentWords, 8u);
}
