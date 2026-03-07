#include "Backend/BackendFactory.hpp"

#include <gtest/gtest.h>

TEST(BackendFactory, CreatesCpuFallbackByDefault)
{
    EXPECT_EQ(backend::BackendKind::CPU, backend::defaultBackendKind());
}
