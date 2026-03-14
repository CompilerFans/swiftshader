#include "Backend/BackendFactory.hpp"
#include "Backend/PresentAdapter.hpp"

#include <gtest/gtest.h>

TEST(PresentAdapterFactory, SelectsExpectedAdapterKind)
{
    auto adapter = backend::createPresentAdapter();
    ASSERT_NE(adapter, nullptr);
#if SWIFTSHADER_ENABLE_GPU_BACKEND
    EXPECT_FALSE(adapter->isFallbackAdapter());
#else
    EXPECT_TRUE(adapter->isFallbackAdapter());
#endif
}

TEST(PresentAdapterFactory, UpdatesLayoutOnAcquireAndPresent)
{
    auto adapter = backend::createPresentAdapter();
    backend::ResourceStateTracker tracker;
    adapter->acquire(tracker, 7);
    EXPECT_EQ(tracker.layoutForImage(7), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    adapter->present(tracker, 7);
    EXPECT_EQ(tracker.layoutForImage(7), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

#if SWIFTSHADER_ENABLE_GPU_BACKEND
TEST(PresentAdapterFactory, GpuAdapterCapturesAcquireAndPresent)
{
	backend::resetPresentAdapterCapture();
	auto adapter = backend::createPresentAdapter();
	backend::ResourceStateTracker tracker;
	adapter->acquire(tracker, 11);
	adapter->present(tracker, 11);
	auto capture = backend::lastPresentAdapterCapture();
	EXPECT_EQ(capture.acquireCount, 1u);
	EXPECT_EQ(capture.presentCount, 1u);
	EXPECT_EQ(capture.lastAcquireImageId, 11u);
	EXPECT_EQ(capture.lastPresentImageId, 11u);
	EXPECT_EQ(capture.lastAcquireLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	EXPECT_EQ(capture.lastPresentLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
#endif
