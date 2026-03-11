#include "PresentAdapter.hpp"

#include "Backend/BackendConfig.hpp"

namespace backend {
namespace {

PresentAdapterCapture gCapture = {};

class FallbackPresentAdapter : public PresentAdapter
{
public:
	bool isFallbackAdapter() const override
	{
		return true;
	}

	void acquire(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		tracker.transitionImage(imageId, tracker.layoutForImage(imageId), VK_IMAGE_LAYOUT_GENERAL);
	}

	void present(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		tracker.transitionImage(imageId, tracker.layoutForImage(imageId), VK_IMAGE_LAYOUT_GENERAL);
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
		tracker.transitionImage(imageId, tracker.layoutForImage(imageId), VK_IMAGE_LAYOUT_GENERAL);
		gCapture.acquireCount++;
		gCapture.lastAcquireImageId = imageId;
	}

	void present(ResourceStateTracker &tracker, uint64_t imageId) override
	{
		tracker.transitionImage(imageId, tracker.layoutForImage(imageId), VK_IMAGE_LAYOUT_GENERAL);
		gCapture.presentCount++;
		gCapture.lastPresentImageId = imageId;
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
