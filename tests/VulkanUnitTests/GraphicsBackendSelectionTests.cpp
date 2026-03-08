#include "Backend/BackendFactory.hpp"
#include "Backend/GraphicsBackend.hpp"

#include <gtest/gtest.h>

TEST(GraphicsBackendSelection, DefaultsToCpuRendererForGraphics)
{
	EXPECT_EQ(backend::defaultBackendKind(), backend::BackendKind::CPU);
}
