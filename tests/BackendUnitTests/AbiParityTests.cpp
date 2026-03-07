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
