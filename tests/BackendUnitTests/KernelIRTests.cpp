#include "Pipeline/KernelABI.hpp"
#include "Pipeline/KernelIR.hpp"
#include "Pipeline/KernelIRLowering.hpp"
#include "Pipeline/SemanticIR.hpp"

#include <gtest/gtest.h>

#include <type_traits>

TEST(KernelABI, DefaultHeaderIsPodCompatible)
{
    EXPECT_TRUE(std::is_standard_layout<sw::KernelABIHeader>::value);
    EXPECT_TRUE(std::is_trivial<sw::KernelABIHeader>::value);
}

TEST(KernelIR, PreservesQuadAndHelperLaneMetadata)
{
    sw::KernelIRModule module;
    sw::FragmentExecutionInfo fragmentInfo{};
    fragmentInfo.quadWidth = 2;
    fragmentInfo.quadHeight = 2;
    fragmentInfo.helperLaneMask = 0xA;
    fragmentInfo.exportMask = 0x5;

    module.setFragmentExecutionInfo(fragmentInfo);

    EXPECT_EQ(module.fragmentExecutionInfo().quadWidth, 2u);
    EXPECT_EQ(module.fragmentExecutionInfo().quadHeight, 2u);
    EXPECT_EQ(module.fragmentExecutionInfo().helperLaneMask, 0xAu);
    EXPECT_EQ(module.fragmentExecutionInfo().exportMask, 0x5u);
}

TEST(KernelIR, LowersMinimalVertexSemanticInfo)
{
    sw::VertexLoweringInfo vertexInfo{};
    vertexInfo.usesPositionAttribute = true;
    vertexInfo.positionAttributeLocation = 0;
    vertexInfo.positionBinding = 0;
    vertexInfo.positionInputComponentCount = 3;
    vertexInfo.vertexStride = 12;
    vertexInfo.positionOffset = 4;
    vertexInfo.usesVertexIndex = true;

    sw::SemanticIRModule semantic(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexInfo);
    sw::KernelIRModule kernel = sw::lowerToKernelIR(semantic);

    EXPECT_TRUE(kernel.hasVertexLoweringInfo());
    EXPECT_TRUE(kernel.vertexLoweringInfo().usesPositionAttribute);
    EXPECT_EQ(kernel.vertexLoweringInfo().positionAttributeLocation, 0u);
    EXPECT_EQ(kernel.vertexLoweringInfo().positionBinding, 0u);
    EXPECT_EQ(kernel.vertexLoweringInfo().positionInputComponentCount, 3u);
    EXPECT_EQ(kernel.vertexLoweringInfo().vertexStride, 12u);
    EXPECT_EQ(kernel.vertexLoweringInfo().positionOffset, 4u);
    EXPECT_TRUE(kernel.vertexLoweringInfo().usesVertexIndex);
}
