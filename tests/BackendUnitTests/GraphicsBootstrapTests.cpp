#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/GraphicsBootstrap.hpp"

#include <gtest/gtest.h>

TEST(GraphicsBootstrap, EmitsVertexStyleCudaSource)
{
	std::string source = backend::graphicsBootstrapCudaSource();

	EXPECT_NE(source.find("struct VertexInput"), std::string::npos);
	EXPECT_NE(source.find("struct VertexOutput"), std::string::npos);
	EXPECT_NE(source.find("outVertices[vertexIndex].w = 1.0f;"), std::string::npos);
}

TEST(GraphicsBootstrap, LaunchUsesThreeArguments)
{
	backend::FakeRuntimeAPI runtime;
	backend::launchGraphicsBootstrap(runtime);

	EXPECT_NE(runtime.lastModuleSource().find("struct VertexInput"), std::string::npos);
	EXPECT_EQ(runtime.lastLaunch().argumentCount, 3u);
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 1u);
	EXPECT_EQ(runtime.lastLaunch().blockCountX, 1u);
}
