#include "GraphicsDraw.hpp"

namespace backend {

GraphicsDrawRoute chooseGraphicsDrawRoute(bool hasRuntime,
                                          bool hardwareBacked,
                                          bool allowCpuFallback,
                                          bool renderTriangleBootstrap,
                                          bool rasterizerDiscard)
{
	(void)renderTriangleBootstrap;

	if(rasterizerDiscard || !hasRuntime || !hardwareBacked)
	{
		return GraphicsDrawRoute::CpuRenderer;
	}

	if(!allowCpuFallback)
	{
		return GraphicsDrawRoute::GpuBootstrapRequired;
	}

	return GraphicsDrawRoute::GpuBootstrapOptional;
}

}  // namespace backend
