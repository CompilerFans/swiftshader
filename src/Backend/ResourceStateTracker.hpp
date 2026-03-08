#ifndef SWIFTSHADER_RESOURCE_STATE_TRACKER_HPP_
#define SWIFTSHADER_RESOURCE_STATE_TRACKER_HPP_

#include "Vulkan/VulkanPlatform.hpp"

#include <cstdint>
#include <unordered_map>

namespace backend {

class ResourceStateTracker
{
public:
	void transitionImage(uint64_t imageId, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		(void)oldLayout;
		imageLayouts[imageId] = newLayout;
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
