#include "Backend/BackendFactory.hpp"

#include <gtest/gtest.h>

TEST(BackendSelection, DefaultsToCpuBackend)
{
    EXPECT_EQ(backend::BackendKind::CPU, backend::defaultBackendKind());
}
