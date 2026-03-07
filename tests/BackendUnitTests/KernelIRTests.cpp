#include "Pipeline/KernelABI.hpp"
#include "Pipeline/KernelIR.hpp"

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
