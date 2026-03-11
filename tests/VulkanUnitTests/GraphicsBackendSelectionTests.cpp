#include "Backend/BackendFactory.hpp"
#include "Backend/GraphicsBackend.hpp"

#include <gtest/gtest.h>

TEST(GraphicsBackendSelection, DefaultsToCpuRendererForGraphics)
{
#if SWIFTSHADER_ENABLE_GPU_BACKEND
	EXPECT_EQ(backend::defaultBackendKind(), backend::BackendKind::GPU);
#else
	EXPECT_EQ(backend::defaultBackendKind(), backend::BackendKind::CPU);
#endif
}
