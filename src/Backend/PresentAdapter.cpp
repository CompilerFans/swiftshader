#include "PresentAdapter.hpp"

namespace backend {
namespace {

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

}  // namespace

std::unique_ptr<PresentAdapter> createFallbackPresentAdapter()
{
	return std::make_unique<FallbackPresentAdapter>();
}

}  // namespace backend
