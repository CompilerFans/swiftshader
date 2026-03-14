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

TEST(TriangleBootstrapDrawPolicy, AcceptsStoreLayoutForPresentAttachment)
{
	backend::GraphicsColorAttachmentTarget target = {};
	target.imageView = reinterpret_cast<vk::ImageView *>(0x1);
	target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	target.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	target.present = true;

	EXPECT_TRUE(backend::canWriteTriangleBootstrapColorAttachment(target));
}

TEST(TriangleBootstrapDrawPolicy, AcceptsGeneralLayoutForPresentAttachment)
{
	backend::GraphicsColorAttachmentTarget target = {};
	target.imageView = reinterpret_cast<vk::ImageView *>(0x1);
	target.layout = VK_IMAGE_LAYOUT_GENERAL;
	target.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	target.present = true;

	EXPECT_TRUE(backend::canWriteTriangleBootstrapColorAttachment(target));
}

TEST(TriangleBootstrapDrawPolicy, RejectsDontCareStoreOp)
{
	backend::GraphicsColorAttachmentTarget target = {};
	target.imageView = reinterpret_cast<vk::ImageView *>(0x1);
	target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	target.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	target.present = true;

	EXPECT_FALSE(backend::canWriteTriangleBootstrapColorAttachment(target));
}

TEST(TriangleBootstrapDrawPolicy, RejectsNonWritableLayout)
{
	backend::GraphicsColorAttachmentTarget target = {};
	target.imageView = reinterpret_cast<vk::ImageView *>(0x1);
	target.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	target.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	target.present = true;

	EXPECT_FALSE(backend::canWriteTriangleBootstrapColorAttachment(target));
}
