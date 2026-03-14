#ifndef SWIFTSHADER_GRAPHICS_DRAW_HPP_
#define SWIFTSHADER_GRAPHICS_DRAW_HPP_

#include "Vulkan/VulkanPlatform.hpp"

namespace sw {
class CountedEvent;
}

namespace vk {
class DynamicState;
class GraphicsPipeline;
class ImageView;
}  // namespace vk

namespace backend {

enum class GraphicsDrawRoute
{
	CpuRenderer,
	GpuBootstrapOptional,
	GpuBootstrapRequired,
};

struct GraphicsColorAttachmentTarget
{
	vk::ImageView *imageView = nullptr;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	bool present = false;
};

struct GraphicsDrawCall
{
	const vk::GraphicsPipeline *pipeline = nullptr;
	const vk::DynamicState *dynamicState = nullptr;
	unsigned int count = 0;
	int baseVertex = 0;
	sw::CountedEvent *events = nullptr;
	int instanceID = 0;
	int layer = 0;
	void *indexBuffer = nullptr;
	VkRect2D renderArea = {};
	GraphicsColorAttachmentTarget colorAttachment0 = {};
	const void *pushConstants = nullptr;
};

GraphicsDrawRoute chooseGraphicsDrawRoute(bool hasRuntime,
                                          bool hardwareBacked,
                                          bool allowCpuFallback,
                                          bool renderTriangleBootstrap,
                                          bool rasterizerDiscard);

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_DRAW_HPP_
