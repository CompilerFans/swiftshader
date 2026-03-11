#include "Backend/BackendFactory.hpp"

#include <gtest/gtest.h>

TEST(BackendSelection, DefaultsToCpuBackend)
{
#if SWIFTSHADER_ENABLE_GPU_BACKEND
	EXPECT_EQ(backend::BackendKind::GPU, backend::defaultBackendKind());
#else
	EXPECT_EQ(backend::BackendKind::CPU, backend::defaultBackendKind());
#endif
}
