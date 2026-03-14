#include "Backend/BackendFactory.hpp"
#include "Backend/PresentAdapter.hpp"
#include "Device.hpp"
#include "Driver.hpp"

#include "gtest/gtest.h"

class PresentAdapterCaptureTest : public testing::Test
{
protected:
	static Driver driver;

	static void SetUpTestSuite()
	{
		ASSERT_TRUE(driver.loadSwiftShader());
	}

	static void TearDownTestSuite()
	{
		driver.unload();
	}
};

Driver PresentAdapterCaptureTest::driver;

#if SWIFTSHADER_ENABLE_GPU_BACKEND
TEST_F(PresentAdapterCaptureTest, GpuPresentAdapterCaptureApiWorks)
{
	backend::resetPresentAdapterCapture();
	backend::ResourceStateTracker tracker;
	auto adapter = backend::createPresentAdapter();
	ASSERT_NE(adapter, nullptr);
	adapter->acquire(tracker, 21);
	adapter->present(tracker, 21);
	const auto &capture = backend::lastPresentAdapterCapture();
	EXPECT_EQ(capture.acquireCount, 1u);
	EXPECT_EQ(capture.presentCount, 1u);
	EXPECT_EQ(capture.lastAcquireImageId, 21u);
	EXPECT_EQ(capture.lastPresentImageId, 21u);
	EXPECT_EQ(capture.lastAcquireLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	EXPECT_EQ(capture.lastPresentLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
#endif
