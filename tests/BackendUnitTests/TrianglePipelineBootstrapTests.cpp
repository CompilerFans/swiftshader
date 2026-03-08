#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/TrianglePipelineBootstrap.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

#include <cstring>
#include <gtest/gtest.h>

TEST(TrianglePipelineBootstrap, FakeRuntimeLaunchesThreeStages)
{
	backend::FakeRuntimeAPI runtime;
	backend::FakeRuntimeAPI::resetGlobalCapture();

	backend::launchTrianglePipelineBootstrap(runtime);

	EXPECT_EQ(backend::FakeRuntimeAPI::globalLaunchCount(), 3u);
	EXPECT_NE(backend::FakeRuntimeAPI::globalLastModuleSource().find("struct FsParams"), std::string::npos);
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(TrianglePipelineBootstrap, CudaRuntimeProducesGreenTriangleColorBuffer)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<uint8_t> colorBuffer;
	backend::CudaRuntimeAPI::resetGlobalCapture();
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, 64u, 64u, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);
	EXPECT_EQ(backend::CudaRuntimeAPI::globalLaunchCount(), 3u);

	size_t inside = ((32u * 64u) + 32u) * 4u;
	EXPECT_EQ(colorBuffer[inside + 0], 0u);
	EXPECT_EQ(colorBuffer[inside + 1], 255u);
	EXPECT_EQ(colorBuffer[inside + 2], 0u);
	EXPECT_EQ(colorBuffer[inside + 3], 255u);

	size_t outside = ((2u * 64u) + 2u) * 4u;
	EXPECT_EQ(colorBuffer[outside + 0], 0u);
	EXPECT_EQ(colorBuffer[outside + 1], 0u);
	EXPECT_EQ(colorBuffer[outside + 2], 0u);
	EXPECT_EQ(colorBuffer[outside + 3], 0u);
}

TEST(TrianglePipelineBootstrap, CudaRuntimeAppliesRequestedFragmentColor)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	backend::TrianglePipelineBootstrapConfig config = {};
	config.width = 64u;
	config.height = 64u;
	config.colorR = 1.0f;
	config.colorG = 0.0f;
	config.colorB = 0.0f;
	config.colorA = 1.0f;

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);

	size_t inside = ((32u * 64u) + 32u) * 4u;
	EXPECT_EQ(colorBuffer[inside + 0], 255u);
	EXPECT_EQ(colorBuffer[inside + 1], 0u);
	EXPECT_EQ(colorBuffer[inside + 2], 0u);
	EXPECT_EQ(colorBuffer[inside + 3], 255u);
}

TEST(TrianglePipelineBootstrap, CudaRuntimeAppliesRequestedVertexGeometry)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	backend::TrianglePipelineBootstrapConfig config = {};
	config.width = 64u;
	config.height = 64u;
	config.colorR = 1.0f;
	config.colorG = 0.0f;
	config.colorB = 0.0f;
	config.colorA = 1.0f;
	config.vertices[0] = { 0.10f, -0.25f, 0.0f };
	config.vertices[1] = { 0.60f, 0.75f, 0.0f };
	config.vertices[2] = { 0.90f, -0.25f, 0.0f };

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);

	size_t shiftedInside = ((32u * 64u) + 48u) * 4u;
	EXPECT_EQ(colorBuffer[shiftedInside + 0], 255u);
	EXPECT_EQ(colorBuffer[shiftedInside + 1], 0u);
	EXPECT_EQ(colorBuffer[shiftedInside + 2], 0u);
	EXPECT_EQ(colorBuffer[shiftedInside + 3], 255u);

	size_t leftOutside = ((32u * 64u) + 16u) * 4u;
	EXPECT_EQ(colorBuffer[leftOutside + 0], 0u);
	EXPECT_EQ(colorBuffer[leftOutside + 1], 0u);
	EXPECT_EQ(colorBuffer[leftOutside + 2], 0u);
	EXPECT_EQ(colorBuffer[leftOutside + 3], 0u);
}

TEST(TrianglePipelineBootstrap, CudaRuntimeUsesRawVertexDataAndBinding)
{
	struct InterleavedVertex
	{
		uint32_t tag;
		float position[3];
		uint32_t tail;
	};

	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<InterleavedVertex> vertices = {
		{ 11u, { 0.10f, -0.25f, 0.0f }, 101u },
		{ 22u, { 0.60f, 0.75f, 0.0f }, 202u },
		{ 33u, { 0.90f, -0.25f, 0.0f }, 303u },
	};

	backend::TrianglePipelineBootstrapConfig config = {};
	config.width = 64u;
	config.height = 64u;
	config.colorR = 1.0f;
	config.colorG = 0.0f;
	config.colorB = 0.0f;
	config.colorA = 1.0f;
	config.rawVertexData.resize(sizeof(InterleavedVertex) * vertices.size());
	std::memcpy(config.rawVertexData.data(), vertices.data(), config.rawVertexData.size());
	config.vertexCount = static_cast<uint32_t>(vertices.size());
	config.binding.vertexStride = sizeof(InterleavedVertex);
	config.binding.positionOffset = offsetof(InterleavedVertex, position);

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);

	size_t shiftedInside = ((32u * 64u) + 48u) * 4u;
	EXPECT_EQ(colorBuffer[shiftedInside + 0], 255u);
	EXPECT_EQ(colorBuffer[shiftedInside + 1], 0u);
	EXPECT_EQ(colorBuffer[shiftedInside + 2], 0u);
	EXPECT_EQ(colorBuffer[shiftedInside + 3], 255u);
}
#endif
