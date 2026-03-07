#include "Pipeline/CudaLikeSourceEmitter.hpp"
#include "Pipeline/LlvmIREmitter.hpp"

#include <gtest/gtest.h>

TEST(CodegenEmitter, EmitsCudaLikeKernelSignature)
{
    sw::KernelIRModule module;
    std::string text = sw::emitCudaLikeSource(module);
    EXPECT_NE(text.find("extern \"C\""), std::string::npos);
}

TEST(CodegenEmitter, EmitsLlvmIRHeader)
{
    sw::KernelIRModule module;
    std::string text = sw::emitLlvmIR(module);
    EXPECT_NE(text.find("define"), std::string::npos);
}
