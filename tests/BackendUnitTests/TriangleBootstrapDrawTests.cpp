#include "Backend/TriangleBootstrapDraw.hpp"

#include <gtest/gtest.h>

TEST(TriangleBootstrapDrawPlan, UsesWarmupOnlyPassBeforeBootstrapHasCompleted)
{
	auto plan = backend::planTriangleBootstrapDraw(backend::GraphicsDrawRoute::GpuBootstrapOptional,
	                                               false, false, false);

	EXPECT_EQ(plan.pass, backend::TriangleBootstrapPass::WarmupOnly);
	EXPECT_FALSE(plan.requireSuccessfulWriteback);
}

TEST(TriangleBootstrapDrawPlan, SkipsWarmupOnceBootstrapHasCompleted)
{
	auto plan = backend::planTriangleBootstrapDraw(backend::GraphicsDrawRoute::GpuBootstrapOptional,
	                                               false, false, true);

	EXPECT_EQ(plan.pass, backend::TriangleBootstrapPass::Skip);
	EXPECT_FALSE(plan.requireSuccessfulWriteback);
}

TEST(TriangleBootstrapDrawPlan, RequiresAttachmentWriteWhenGpuPathIsStrict)
{
	auto plan = backend::planTriangleBootstrapDraw(backend::GraphicsDrawRoute::GpuBootstrapRequired,
	                                               false, false, false);

	EXPECT_EQ(plan.pass, backend::TriangleBootstrapPass::RenderToColorAttachment);
	EXPECT_TRUE(plan.requireSuccessfulWriteback);
}

TEST(TriangleBootstrapDrawPlan, PropagatesExplicitRenderRequirement)
{
	auto plan = backend::planTriangleBootstrapDraw(backend::GraphicsDrawRoute::GpuBootstrapOptional,
	                                               true, true, false);

	EXPECT_EQ(plan.pass, backend::TriangleBootstrapPass::RenderToColorAttachment);
	EXPECT_TRUE(plan.requireSuccessfulWriteback);
}
