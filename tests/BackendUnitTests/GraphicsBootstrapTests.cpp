#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/GraphicsBootstrap.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

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

TEST(GraphicsBootstrap, EmitsOffsetShaderBody)
{
	backend::GraphicsBootstrapShaderConfig config = {};
	config.offsetX = 0.25f;
	config.offsetY = -0.5f;

	std::string source = backend::graphicsBootstrapCudaSource(config);

	EXPECT_NE(source.find("outVertex.x = inVertex.x + 0.25f + params.runtimeOffsetX;"), std::string::npos);
	EXPECT_NE(source.find("outVertex.y = inVertex.y + -0.5f + params.runtimeOffsetY;"), std::string::npos);
	EXPECT_NE(source.find("outVertex.z = inVertex.z + 0.0f + params.runtimeOffsetZ;"), std::string::npos);
}

TEST(GraphicsBootstrap, EmitsVertexIndexShaderBody)
{
	backend::GraphicsBootstrapShaderConfig config = {};
	config.vertexIndexScaleX = 0.5f;

	std::string source = backend::graphicsBootstrapCudaSource(config);

	EXPECT_NE(source.find("static_cast<float>(vertexIndex) * 0.5f"), std::string::npos);
}

TEST(GraphicsBootstrap, EmitsRuntimeOffsetParams)
{
	std::string source = backend::graphicsBootstrapCudaSource();

	EXPECT_NE(source.find("float runtimeOffsetX;"), std::string::npos);
	EXPECT_NE(source.find("float runtimeOffsetY;"), std::string::npos);
	EXPECT_NE(source.find("float runtimeOffsetZ;"), std::string::npos);
	EXPECT_NE(source.find("outVertex.x = inVertex.x + 0.0f + params.runtimeOffsetX;"), std::string::npos);
}

TEST(GraphicsBootstrap, LaunchUsesSingleVsParamsArgument)
{
	backend::FakeRuntimeAPI runtime;
	backend::launchGraphicsBootstrap(runtime);

	EXPECT_NE(runtime.lastModuleSource().find("struct VsParams"), std::string::npos);
	EXPECT_EQ(runtime.lastLaunch().argumentCount, 1u);
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 1u);
	EXPECT_EQ(runtime.lastLaunch().blockCountX, 3u);
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(GraphicsBootstrap, CudaRuntimeExecutesThreeVertices)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::GraphicsBootstrapVertexInput> inputs = {
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	};
	std::vector<backend::GraphicsBootstrapVertexOutput> outputs;

	ASSERT_TRUE(backend::runGraphicsBootstrap(runtime, inputs, &outputs));
	ASSERT_EQ(outputs.size(), inputs.size());

	for(size_t i = 0; i < inputs.size(); i++)
	{
		EXPECT_FLOAT_EQ(outputs[i].x, inputs[i].x);
		EXPECT_FLOAT_EQ(outputs[i].y, inputs[i].y);
		EXPECT_FLOAT_EQ(outputs[i].z, inputs[i].z);
		EXPECT_FLOAT_EQ(outputs[i].w, 1.0f);
	}
}

TEST(GraphicsBootstrap, CudaRuntimeExecutesOffsetVertices)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::GraphicsBootstrapVertexInput> inputs = {
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	};
	std::vector<backend::GraphicsBootstrapVertexOutput> outputs;

	backend::GraphicsBootstrapShaderConfig config = {};
	config.offsetX = 0.25f;
	config.offsetY = -0.5f;
	config.offsetZ = 0.125f;

	ASSERT_TRUE(backend::runGraphicsBootstrap(runtime, inputs, config, &outputs));
	ASSERT_EQ(outputs.size(), inputs.size());

	for(size_t i = 0; i < inputs.size(); i++)
	{
		EXPECT_FLOAT_EQ(outputs[i].x, inputs[i].x + config.offsetX);
		EXPECT_FLOAT_EQ(outputs[i].y, inputs[i].y + config.offsetY);
		EXPECT_FLOAT_EQ(outputs[i].z, inputs[i].z + config.offsetZ);
		EXPECT_FLOAT_EQ(outputs[i].w, 1.0f);
	}
}

TEST(GraphicsBootstrap, CudaRuntimeExecutesVertexIndexShift)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::GraphicsBootstrapVertexInput> inputs = {
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	};
	std::vector<backend::GraphicsBootstrapVertexOutput> outputs;

	backend::GraphicsBootstrapShaderConfig config = {};
	config.vertexIndexScaleX = 0.5f;

	ASSERT_TRUE(backend::runGraphicsBootstrap(runtime, inputs, config, &outputs));
	ASSERT_EQ(outputs.size(), inputs.size());

	for(size_t i = 0; i < inputs.size(); i++)
	{
		EXPECT_FLOAT_EQ(outputs[i].x, inputs[i].x + static_cast<float>(i) * config.vertexIndexScaleX);
		EXPECT_FLOAT_EQ(outputs[i].y, inputs[i].y);
		EXPECT_FLOAT_EQ(outputs[i].z, inputs[i].z);
		EXPECT_FLOAT_EQ(outputs[i].w, 1.0f);
	}
}

TEST(GraphicsBootstrap, CudaRuntimeExecutesRuntimeOffset)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::GraphicsBootstrapVertexInput> inputs = {
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	};
	std::vector<backend::GraphicsBootstrapVertexOutput> outputs;

	backend::GraphicsBootstrapRuntimeConfig runtimeConfig = {};
	runtimeConfig.offsetX = 0.125f;
	runtimeConfig.offsetY = -0.25f;
	runtimeConfig.offsetZ = 0.375f;

	ASSERT_TRUE(backend::runGraphicsBootstrap(runtime, inputs, backend::GraphicsBootstrapShaderConfig{}, runtimeConfig, &outputs));
	ASSERT_EQ(outputs.size(), inputs.size());

	for(size_t i = 0; i < inputs.size(); i++)
	{
		EXPECT_FLOAT_EQ(outputs[i].x, inputs[i].x + runtimeConfig.offsetX);
		EXPECT_FLOAT_EQ(outputs[i].y, inputs[i].y + runtimeConfig.offsetY);
		EXPECT_FLOAT_EQ(outputs[i].z, inputs[i].z + runtimeConfig.offsetZ);
		EXPECT_FLOAT_EQ(outputs[i].w, 1.0f);
	}
}
#endif
