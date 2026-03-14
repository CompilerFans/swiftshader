#include "PresentAdapter.hpp"

#include "Backend/BackendConfig.hpp"

namespace backend {
namespace {

PresentAdapterCapture gCapture = {};

void markSwapchainImagePresented(ResourceStateTracker &tracker, uint64_t imageId)
{
	tracker.transitionImage(imageId, tracker.layoutForImage(imageId), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

class FallbackPresentAdapter : public PresentAdapter
{
public:
	bool isFallbackAdapter() const override
	{
		return true;
	}

	void acquire(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		markSwapchainImagePresented(tracker, imageId);
	}

	void present(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		markSwapchainImagePresented(tracker, imageId);
	}
};

class GpuPresentAdapter : public PresentAdapter
{
public:
	bool isFallbackAdapter() const override
	{
		return false;
	}

	void acquire(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		markSwapchainImagePresented(tracker, imageId);
		gCapture.acquireCount++;
		gCapture.lastAcquireImageId = imageId;
		gCapture.lastAcquireLayout = tracker.layoutForImage(imageId);
	}

	void present(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		markSwapchainImagePresented(tracker, imageId);
		gCapture.presentCount++;
		gCapture.lastPresentImageId = imageId;
		gCapture.lastPresentLayout = tracker.layoutForImage(imageId);
	}
};

}  // namespace

std::unique_ptr<PresentAdapter> createFallbackPresentAdapter()
{
	return std::make_unique<FallbackPresentAdapter>();
}

std::unique_ptr<PresentAdapter> createGpuPresentAdapter()
{
	return std::make_unique<GpuPresentAdapter>();
}

void resetPresentAdapterCapture()
{
	gCapture = {};
}

const PresentAdapterCapture &lastPresentAdapterCapture()
{
	return gCapture;
}

}  // namespace backend
