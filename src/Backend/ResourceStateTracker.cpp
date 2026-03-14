#include "ResourceStateTracker.hpp"

namespace backend {
namespace {

ResourceStateTrackerCapture gCapture = {};

}  // namespace

void resetResourceStateTrackerCapture()
{
	gCapture = {};
}

void recordResourceStateTrackerImageTransition(uint64_t imageId, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	gCapture.imageTransitionCount++;
	gCapture.lastImageId = imageId;
	gCapture.lastOldLayout = oldLayout;
	gCapture.lastNewLayout = newLayout;
}

const ResourceStateTrackerCapture &lastResourceStateTrackerCapture()
{
	return gCapture;
}

}  // namespace backend
