#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/GraphicsBootstrap.hpp"

#include <gtest/gtest.h>

TEST(GraphicsBootstrap, EmitsVertexStageWrapperAndShaderBody)
{
	std::string source = backend::graphicsBootstrapCudaSource();

	EXPECT_NE(source.find("struct VertexInput"), std::string::npos);
	EXPECT_NE(source.find("struct VertexOutput"), std::string::npos);
	EXPECT_NE(source.find("struct VsParams"), std::string::npos);
	EXPECT_NE(source.find("static __device__ void vs_main"), std::string::npos);
	EXPECT_NE(source.find("extern \"C\" __global__ void vs_entry"), std::string::npos);
	EXPECT_NE(source.find("vs_main(params, vertexIndex, inVertex, outVertex);"), std::string::npos);
	EXPECT_NE(source.find("params.outVertices[vertexIndex] = outVertex;"), std::string::npos);
	EXPECT_EQ(source.find("extern \"C\" __global__ void kernel_main"), std::string::npos);
}

TEST(GraphicsBootstrap, LaunchUsesSingleVsParamsArgument)
{
	backend::FakeRuntimeAPI runtime;
	backend::launchGraphicsBootstrap(runtime);

	EXPECT_NE(runtime.lastModuleSource().find("struct VsParams"), std::string::npos);
	EXPECT_EQ(runtime.lastLaunch().argumentCount, 1u);
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 1u);
	EXPECT_EQ(runtime.lastLaunch().blockCountX, 1u);
}
