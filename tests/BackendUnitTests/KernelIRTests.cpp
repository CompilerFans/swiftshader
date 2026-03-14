#include "Pipeline/KernelABI.hpp"
#include "Pipeline/KernelIR.hpp"
#include "Pipeline/KernelIRLowering.hpp"
#include "Pipeline/SemanticIR.hpp"
#include "Pipeline/ShaderCompiler/ShaderCompilerAnalysis.hpp"

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

TEST(KernelIR, PreservesCompilerAnalysisMetadata)
{
    sw::ShaderCompilerAnalysisResult analysis = {};
    analysis.texturePlan.resourceKind = sw::ShaderTextureResourceKind::CombinedImageSampler;
    analysis.imageResourcePlan.sampledDescriptors.push_back({});
    analysis.resourcePlan.descriptorSetCount = 1;
    analysis.resourcePlan.descriptors.push_back({});
    analysis.fragmentFeatureMask = static_cast<uint32_t>(sw::ShaderFragmentFeature::Derivatives);
    analysis.unsupportedReasonMask = static_cast<uint32_t>(sw::ShaderUnsupportedReason::Derivatives);

    sw::KernelIRModule kernel;
    sw::applyCompilerAnalysisToKernelIR(analysis, &kernel);

    EXPECT_EQ(kernel.compilerAnalysisInfo().fragmentFeatureMask,
              static_cast<uint32_t>(sw::ShaderFragmentFeature::Derivatives));
    EXPECT_EQ(kernel.compilerAnalysisInfo().unsupportedReasonMask,
              static_cast<uint32_t>(sw::ShaderUnsupportedReason::Derivatives));
    EXPECT_TRUE(kernel.compilerAnalysisInfo().hasTexturePlan);
    EXPECT_TRUE(kernel.compilerAnalysisInfo().hasImageResourcePlan);
    EXPECT_TRUE(kernel.compilerAnalysisInfo().hasResourcePlan);
}
