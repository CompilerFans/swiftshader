#include "Pipeline/SemanticIR.hpp"

#include <gtest/gtest.h>

TEST(SemanticIR, ModuleStoresStageAndEntryPoint)
{
    sw::SemanticIRModule module(VK_SHADER_STAGE_COMPUTE_BIT, "main");
    EXPECT_EQ(module.stage(), VK_SHADER_STAGE_COMPUTE_BIT);
    EXPECT_EQ(module.entryPoint(), "main");
}

TEST(SemanticIR, DistinguishesCombinedSeparateAndStorageResources)
{
    EXPECT_NE(sw::ResourceAccessKind::CombinedImageSampler,
              sw::ResourceAccessKind::StorageImage);
}
