#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/TrianglePipelineBootstrap.hpp"
#include "Device/Stream.hpp"
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

TEST(TrianglePipelineBootstrap, BuildsConfigFromVec3PositionStream)
{
	struct Vertex
	{
		float position[3];
		float padding;
	};

	const std::array<Vertex, 3> vertices = {{
		{ { -0.5f, -0.25f, 0.0f }, 10.0f },
		{ { 0.0f, 0.75f, 0.0f }, 20.0f },
		{ { 0.5f, -0.25f, 0.0f }, 30.0f },
	}};

	sw::Stream positionStream = {};
	positionStream.buffer = vertices.data();
	positionStream.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	positionStream.vertexStride = sizeof(Vertex);
	positionStream.format = VK_FORMAT_R32G32B32_SFLOAT;

	backend::TrianglePipelineBootstrapConfig config = {};
	ASSERT_TRUE(backend::buildTrianglePipelineBootstrapConfig(positionStream, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u, { { 0, 0 }, { 96, 48 } }, &config));
	EXPECT_EQ(config.width, 96u);
	EXPECT_EQ(config.height, 48u);
	EXPECT_EQ(config.vertexCount, 3u);
	EXPECT_EQ(config.binding.vertexStride, sizeof(Vertex));
	EXPECT_EQ(config.binding.positionOffset, 0u);
	EXPECT_EQ(config.binding.positionComponentCount, 3u);
	ASSERT_EQ(config.rawVertexData.size(), sizeof(Vertex) * vertices.size());
	EXPECT_EQ(std::memcmp(config.rawVertexData.data(), vertices.data(), config.rawVertexData.size()), 0);
}

TEST(TrianglePipelineBootstrap, BuildsConfigFromVec2PositionStream)
{
	struct Vertex
	{
		uint32_t tag;
		float position[2];
		uint32_t tail;
	};

	const std::array<Vertex, 3> vertices = {{
		{ 1u, { -0.5f, -0.25f }, 11u },
		{ 2u, { 0.0f, 0.75f }, 22u },
		{ 3u, { 0.5f, -0.25f }, 33u },
	}};

	sw::Stream positionStream = {};
	positionStream.buffer = vertices.data();
	positionStream.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	positionStream.vertexStride = sizeof(Vertex);
	positionStream.format = VK_FORMAT_R32G32_SFLOAT;

	backend::TrianglePipelineBootstrapConfig config = {};
	ASSERT_TRUE(backend::buildTrianglePipelineBootstrapConfig(positionStream, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u, { { 0, 0 }, { 64, 64 } }, &config));
	EXPECT_EQ(config.vertexCount, 3u);
	EXPECT_EQ(config.binding.positionComponentCount, 2u);
	ASSERT_EQ(config.rawVertexData.size(), sizeof(Vertex) * vertices.size());
	EXPECT_EQ(std::memcmp(config.rawVertexData.data(), vertices.data(), config.rawVertexData.size()), 0);
}

TEST(TrianglePipelineBootstrap, BuildsConfigFromMultipleTrianglesPositionStream)
{
	struct Vertex
	{
		float position[3];
	};

	const std::array<Vertex, 6> vertices = {{
		{ { -0.9f, -0.8f, 0.0f } },
		{ { -0.3f, 0.6f, 0.0f } },
		{ { -0.1f, -0.8f, 0.0f } },
		{ { 0.1f, -0.8f, 0.0f } },
		{ { 0.3f, 0.6f, 0.0f } },
		{ { 0.9f, -0.8f, 0.0f } },
	}};

	sw::Stream positionStream = {};
	positionStream.buffer = vertices.data();
	positionStream.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	positionStream.vertexStride = sizeof(Vertex);
	positionStream.format = VK_FORMAT_R32G32B32_SFLOAT;

	backend::TrianglePipelineBootstrapConfig config = {};
	ASSERT_TRUE(backend::buildTrianglePipelineBootstrapConfig(positionStream, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 2u, { { 0, 0 }, { 96, 48 } }, &config));
	EXPECT_EQ(config.vertexCount, 6u);
	ASSERT_EQ(config.rawVertexData.size(), sizeof(Vertex) * vertices.size());
	EXPECT_EQ(std::memcmp(config.rawVertexData.data(), vertices.data(), config.rawVertexData.size()), 0);
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(TrianglePipelineBootstrap, CudaRuntimeProducesGreenTriangleColorBuffer)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();
	backend::CudaRuntimeAPI::resetGlobalCapture();

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

TEST(TrianglePipelineBootstrap, CudaRuntimeRendersMultipleTrianglesFromRawVertexData)
{
	struct Vertex
	{
		float position[3];
	};

	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	const std::array<Vertex, 6> vertices = {{
		{ { -0.9f, -0.8f, 0.0f } },
		{ { -0.3f, 0.6f, 0.0f } },
		{ { -0.1f, -0.8f, 0.0f } },
		{ { 0.1f, -0.8f, 0.0f } },
		{ { 0.3f, 0.6f, 0.0f } },
		{ { 0.9f, -0.8f, 0.0f } },
	}};

	backend::TrianglePipelineBootstrapConfig config = {};
	config.width = 96u;
	config.height = 64u;
	config.rawVertexData.resize(sizeof(Vertex) * vertices.size());
	std::memcpy(config.rawVertexData.data(), vertices.data(), config.rawVertexData.size());
	config.vertexCount = static_cast<uint32_t>(vertices.size());
	config.binding.vertexStride = sizeof(Vertex);
	config.binding.positionOffset = 0u;
	config.binding.positionComponentCount = 3u;

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 96u * 64u * 4u);

	size_t leftInside = ((44u * 96u) + 18u) * 4u;
	EXPECT_EQ(colorBuffer[leftInside + 0], 0u);
	EXPECT_EQ(colorBuffer[leftInside + 1], 255u);
	EXPECT_EQ(colorBuffer[leftInside + 2], 0u);
	EXPECT_EQ(colorBuffer[leftInside + 3], 255u);

	size_t rightInside = ((44u * 96u) + 78u) * 4u;
	EXPECT_EQ(colorBuffer[rightInside + 0], 0u);
	EXPECT_EQ(colorBuffer[rightInside + 1], 255u);
	EXPECT_EQ(colorBuffer[rightInside + 2], 0u);
	EXPECT_EQ(colorBuffer[rightInside + 3], 255u);
}

TEST(TrianglePipelineBootstrap, CudaRuntimeAppliesFragCoordQuadrantFragmentMode)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	backend::TrianglePipelineBootstrapConfig config = {};
	config.width = 64u;
	config.height = 64u;
	config.fragmentConfig.shaderKind = backend::FragmentBootstrapShaderKind::FragCoordQuadrants;

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runTrianglePipelineBootstrap(runtime, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 64u * 64u * 4u);

	size_t topRightInside = ((28u * 64u) + 34u) * 4u;
	EXPECT_EQ(colorBuffer[topRightInside + 0], 0u);
	EXPECT_EQ(colorBuffer[topRightInside + 1], 255u);
	EXPECT_EQ(colorBuffer[topRightInside + 2], 0u);
	EXPECT_EQ(colorBuffer[topRightInside + 3], 255u);
	EXPECT_NE(backend::CudaRuntimeAPI::globalLastModuleSource().find("bool left = invocation.x * 2u < params.width"), std::string::npos);
}
#endif
