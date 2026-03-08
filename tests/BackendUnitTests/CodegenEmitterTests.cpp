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

TEST(CodegenEmitter, EmitsVertexStageCudaLikeSource)
{
    sw::KernelIRModule module;
    sw::VertexLoweringInfo vertexInfo{};
    vertexInfo.usesPositionAttribute = true;
    vertexInfo.positionAttributeLocation = 0;
    vertexInfo.positionBinding = 0;
    vertexInfo.vertexStride = 12;
    vertexInfo.positionOffset = 0;
    vertexInfo.usesVertexIndex = true;
    vertexInfo.usesInstanceIndex = true;
    vertexInfo.constantOffsetX = 0.25f;
    module.setVertexLoweringInfo(vertexInfo);

    std::string text = sw::emitCudaLikeSource(module);
    EXPECT_NE(text.find("extern \"C\" __global__ void vs_entry"), std::string::npos);
    EXPECT_NE(text.find("struct VsParams"), std::string::npos);
    EXPECT_NE(text.find("params.vertexData + vertexIndex * params.vertexStride + params.positionOffset"), std::string::npos);
    EXPECT_NE(text.find("static_cast<float>(vertexIndex)"), std::string::npos);
    EXPECT_NE(text.find("static_cast<float>(params.instanceIndex)"), std::string::npos);
    EXPECT_NE(text.find("outVertex.x += 0.25f;"), std::string::npos);
}
