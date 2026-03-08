#include "Backend/BackendFactory.hpp"

#include <gtest/gtest.h>

TEST(BackendFactory, SelectsExpectedDefaultBackend)
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
    EXPECT_EQ(backend::BackendKind::CUSTOM_GPU, backend::defaultBackendKind());
#else
    EXPECT_EQ(backend::BackendKind::CPU, backend::defaultBackendKind());
#endif
}
