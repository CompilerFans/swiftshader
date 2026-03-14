#ifndef SWIFTSHADER_TRIANGLE_BOOTSTRAP_DRAW_HPP_
#define SWIFTSHADER_TRIANGLE_BOOTSTRAP_DRAW_HPP_

#include "Backend/GraphicsDraw.hpp"

namespace vk {
class Device;
}

namespace backend {

class RuntimeAPI;

enum class TriangleBootstrapPass
{
	Skip,
	WarmupOnly,
	RenderToColorAttachment,
};

struct TriangleBootstrapPlan
{
	TriangleBootstrapPass pass = TriangleBootstrapPass::Skip;
	bool requireSuccessfulWriteback = false;
};

inline bool canWriteTriangleBootstrapColorAttachment(const GraphicsColorAttachmentTarget &target)
{
	if(!target.present || target.imageView == nullptr)
	{
		return false;
	}

	if(target.storeOp != VK_ATTACHMENT_STORE_OP_STORE)
	{
		return false;
	}

	switch(target.layout)
	{
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_GENERAL:
		return true;
	default:
		return false;
	}
}

inline TriangleBootstrapPlan planTriangleBootstrapDraw(GraphicsDrawRoute route,
                                                       bool renderTriangleBootstrap,
                                                       bool requireTriangleBootstrap,
                                                       bool bootstrapAlreadyDone)
{
	TriangleBootstrapPlan plan = {};

	switch(route)
	{
	case GraphicsDrawRoute::CpuRenderer:
		return plan;
	case GraphicsDrawRoute::GpuBootstrapRequired:
		plan.pass = TriangleBootstrapPass::RenderToColorAttachment;
		plan.requireSuccessfulWriteback = true;
		return plan;
	case GraphicsDrawRoute::GpuBootstrapOptional:
		if(renderTriangleBootstrap)
		{
			plan.pass = TriangleBootstrapPass::RenderToColorAttachment;
			plan.requireSuccessfulWriteback = requireTriangleBootstrap;
		}
		else if(!bootstrapAlreadyDone)
		{
			plan.pass = TriangleBootstrapPass::WarmupOnly;
		}
		return plan;
	}

	return plan;
}

bool tryTriangleBootstrapDraw(vk::Device *device,
                              RuntimeAPI &runtime,
                              const GraphicsDrawCall &draw,
                              bool *bootstrapAlreadyDone);

}  // namespace backend

#endif  // SWIFTSHADER_TRIANGLE_BOOTSTRAP_DRAW_HPP_
