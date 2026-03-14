#ifndef SWIFTSHADER_RESOURCE_STATE_TRACKER_HPP_
#define SWIFTSHADER_RESOURCE_STATE_TRACKER_HPP_

#include "Vulkan/VulkanPlatform.hpp"

#include <cstdint>
#include <type_traits>
#include <unordered_map>

namespace backend {

struct ResourceStateTrackerCapture
{
	uint32_t imageTransitionCount = 0;
	uint64_t lastImageId = 0;
	VkImageLayout lastOldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout lastNewLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

void recordResourceStateTrackerImageTransition(uint64_t imageId, VkImageLayout oldLayout, VkImageLayout newLayout);
void resetResourceStateTrackerCapture();
const ResourceStateTrackerCapture &lastResourceStateTrackerCapture();

template<typename Handle>
inline uint64_t trackedResourceHandleId(Handle handle)
{
	using DecayedHandle = std::decay_t<Handle>;
	if constexpr(std::is_pointer_v<DecayedHandle> || std::is_convertible_v<DecayedHandle, void *>)
	{
		return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<void *>(handle)));
	}
	else
	{
		return static_cast<uint64_t>(handle);
	}
}

class ResourceStateTracker
{
public:
	void transitionImage(uint64_t imageId, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		(void)oldLayout;
		imageLayouts[imageId] = newLayout;
		recordResourceStateTrackerImageTransition(imageId, oldLayout, newLayout);
	}

	void transitionImage(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		if(image == VK_NULL_HANDLE)
		{
			return;
		}

		transitionImage(trackedResourceHandleId(image), oldLayout, newLayout);
	}

	void trackDependencyInfo(const VkDependencyInfo &dependencyInfo)
	{
		for(uint32_t i = 0; i < dependencyInfo.imageMemoryBarrierCount; i++)
		{
			const auto &barrier = dependencyInfo.pImageMemoryBarriers[i];
			transitionImage(barrier.image, barrier.oldLayout, barrier.newLayout);
		}
	}

	VkImageLayout layoutForImage(uint64_t imageId) const
	{
		auto it = imageLayouts.find(imageId);
		return (it != imageLayouts.end()) ? it->second : VK_IMAGE_LAYOUT_UNDEFINED;
	}

private:
	std::unordered_map<uint64_t, VkImageLayout> imageLayouts;
};

}  // namespace backend

#endif  // SWIFTSHADER_RESOURCE_STATE_TRACKER_HPP_
