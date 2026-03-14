#include "Backend/GraphicsDraw.hpp"

#include <gtest/gtest.h>

TEST(GraphicsDrawRouting, RequiresGpuBootstrapWhenHardwareRuntimeDisablesCpuFallback)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::GpuBootstrapRequired,
	          backend::chooseGraphicsDrawRoute(true, true, false, false, false));
}

TEST(GraphicsDrawRouting, UsesCpuRendererWhenRuntimeIsUnavailable)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::CpuRenderer,
	          backend::chooseGraphicsDrawRoute(false, false, true, false, false));
}

TEST(GraphicsDrawRouting, UsesOptionalGpuBootstrapWarmupWhenFallbackIsAllowed)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::GpuBootstrapOptional,
	          backend::chooseGraphicsDrawRoute(true, true, true, false, false));
}

TEST(GraphicsDrawRouting, UsesCpuRendererWhenRasterizerDiscardIsEnabled)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::CpuRenderer,
	          backend::chooseGraphicsDrawRoute(true, true, false, true, true));
}

TEST(GraphicsDrawRouting, UsesOptionalGpuBootstrapWhenExplicitlyRequested)
{
	EXPECT_EQ(backend::GraphicsDrawRoute::GpuBootstrapOptional,
	          backend::chooseGraphicsDrawRoute(true, true, true, true, false));
}
