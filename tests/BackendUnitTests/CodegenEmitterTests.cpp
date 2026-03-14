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

TEST(CodegenEmitter, EmitsCompilerAnalysisMetadataInLlvmIR)
{
    sw::KernelIRModule module;
    sw::CompilerAnalysisInfo analysis = {};
    analysis.fragmentFeatureMask = 0x12;
    analysis.unsupportedReasonMask = 0x40;
    analysis.hasTexturePlan = true;
    analysis.hasImageResourcePlan = true;
    analysis.hasResourcePlan = false;
    module.setCompilerAnalysisInfo(analysis);

    std::string text = sw::emitLlvmIR(module);
    EXPECT_NE(text.find("@swiftshader.fragment_feature_mask = internal constant i32 18"), std::string::npos);
    EXPECT_NE(text.find("@swiftshader.unsupported_reason_mask = internal constant i32 64"), std::string::npos);
    EXPECT_NE(text.find("@swiftshader.has_texture_plan = internal constant i1 true"), std::string::npos);
    EXPECT_NE(text.find("@swiftshader.has_image_resource_plan = internal constant i1 true"), std::string::npos);
}

TEST(CodegenEmitter, EmitsVertexStageCudaLikeSource)
{
    sw::KernelIRModule module;
    sw::VertexLoweringInfo vertexInfo{};
    vertexInfo.usesPositionAttribute = true;
    vertexInfo.positionAttributeLocation = 0;
    vertexInfo.positionBinding = 0;
    vertexInfo.positionInputComponentCount = 3;
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

TEST(CodegenEmitter, EmitsVec2VertexInputFallbackForMissingZ)
{
    sw::KernelIRModule module;
    sw::VertexLoweringInfo vertexInfo{};
    vertexInfo.usesPositionAttribute = true;
    vertexInfo.positionAttributeLocation = 0;
    vertexInfo.positionBinding = 0;
    vertexInfo.positionInputComponentCount = 2;
    vertexInfo.vertexStride = 8;
    vertexInfo.positionOffset = 0;
    module.setVertexLoweringInfo(vertexInfo);

    std::string text = sw::emitCudaLikeSource(module);
    EXPECT_NE(text.find("VertexInput inVertex = { position[0], position[1], 0.0f };"), std::string::npos);
    EXPECT_EQ(text.find("position[2]"), std::string::npos);
}

TEST(CodegenEmitter, EmitsVertexStageLlvmIR)
{
    sw::KernelIRModule module;
    sw::VertexLoweringInfo vertexInfo{};
    vertexInfo.usesPositionAttribute = true;
    vertexInfo.positionAttributeLocation = 0;
    vertexInfo.positionBinding = 0;
    vertexInfo.positionInputComponentCount = 3;
    vertexInfo.vertexStride = 12;
    vertexInfo.positionOffset = 0;
    vertexInfo.usesVertexIndex = true;
    module.setVertexLoweringInfo(vertexInfo);

    std::string text = sw::emitLlvmIR(module);
    EXPECT_NE(text.find("%struct.VsParams = type"), std::string::npos);
    EXPECT_NE(text.find("define void @vs_entry(%struct.VsParams* %params)"), std::string::npos);
}
