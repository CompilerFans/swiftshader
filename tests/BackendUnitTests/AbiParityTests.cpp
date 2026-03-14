#include "Pipeline/CudaLikeSourceEmitter.hpp"
#include "Pipeline/LlvmIREmitter.hpp"

#include <gtest/gtest.h>

TEST(AbiParity, NormalizedAbiMatchesAcrossCodegenPaths)
{
    sw::KernelIRModule module;
    sw::FragmentExecutionInfo fragment{};
    fragment.quadWidth = 2;
    fragment.quadHeight = 2;
    fragment.helperLaneMask = 0x3;
    fragment.exportMask = 0xF;
    module.setFragmentExecutionInfo(fragment);

    EXPECT_EQ(sw::describeCudaLikeAbi(module), sw::describeLlvmAbi(module));
}

TEST(AbiParity, PreservesCompilerAnalysisMetadataAcrossCodegenPaths)
{
    sw::KernelIRModule module;
    sw::CompilerAnalysisInfo analysis = {};
    analysis.fragmentFeatureMask = 0x12;
    analysis.unsupportedReasonMask = 0x40;
    analysis.hasTexturePlan = true;
    analysis.hasImageResourcePlan = true;
    module.setCompilerAnalysisInfo(analysis);

    auto cudaAbi = sw::describeCudaLikeAbi(module);
    auto llvmAbi = sw::describeLlvmAbi(module);

    EXPECT_EQ(cudaAbi, llvmAbi);
    EXPECT_EQ(cudaAbi.compilerAnalysis.fragmentFeatureMask, 0x12u);
    EXPECT_EQ(cudaAbi.compilerAnalysis.unsupportedReasonMask, 0x40u);
    EXPECT_TRUE(cudaAbi.compilerAnalysis.hasTexturePlan);
    EXPECT_TRUE(cudaAbi.compilerAnalysis.hasImageResourcePlan);
}
