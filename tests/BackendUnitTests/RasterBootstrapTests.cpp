#include "Backend/StubRuntimeAPI.hpp"
#include "Backend/RasterBootstrap.hpp"
#if SWIFTSHADER_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

#include <array>
#include <gtest/gtest.h>

TEST(RasterBootstrap, EmitsRasterStageWrapperAndShaderBody)
{
	std::string source = backend::rasterBootstrapCudaSource();

	EXPECT_NE(source.find("struct RasterVertex"), std::string::npos);
	EXPECT_NE(source.find("struct RasterInvocation"), std::string::npos);
	EXPECT_NE(source.find("struct RasterParams"), std::string::npos);
	EXPECT_NE(source.find("extern \"C\" __global__ void raster_entry"), std::string::npos);
}

TEST(RasterBootstrap, EmitsBarycentricPayload)
{
	std::string source = backend::rasterBootstrapCudaSource();

	EXPECT_NE(source.find("float barycentric0;"), std::string::npos);
	EXPECT_NE(source.find("float barycentric1;"), std::string::npos);
	EXPECT_NE(source.find("float barycentric2;"), std::string::npos);
	EXPECT_NE(source.find("interpolateBarycentrics"), std::string::npos);
}

TEST(RasterBootstrap, CpuReferenceProducesBoundingBoxAndCoverage)
{
	std::array<backend::RasterBootstrapVertex, 3> triangle = {{
		backend::RasterBootstrapVertex{ 1.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 5.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 1.0f, 5.0f, 0.0f, 1.0f },
	}};

	backend::RasterBootstrapConfig config = {};
	config.width = 8u;
	config.height = 8u;

	backend::RasterBootstrapOutput output = backend::rasterBootstrapCpuReference(triangle, config);

	EXPECT_TRUE(output.valid);
	EXPECT_EQ(output.bboxMinX, 1u);
	EXPECT_EQ(output.bboxMinY, 1u);
	EXPECT_EQ(output.bboxMaxX, 4u);
	EXPECT_EQ(output.bboxMaxY, 4u);
	EXPECT_FALSE(output.invocations.empty());

	bool coversNearOrigin = false;
	bool coversOutside = false;
	for(const auto &invocation : output.invocations)
	{
		coversNearOrigin = coversNearOrigin || (invocation.x == 1u && invocation.y == 1u);
		coversOutside = coversOutside || (invocation.x == 6u || invocation.y == 6u);
		EXPECT_NEAR(invocation.barycentric0 + invocation.barycentric1 + invocation.barycentric2, 1.0f, 0.0001f);
	}

	EXPECT_TRUE(coversNearOrigin);
	EXPECT_FALSE(coversOutside);
}

TEST(RasterBootstrap, LaunchUsesSingleRasterParamsArgument)
{
	backend::StubRuntimeAPI runtime;
	backend::launchRasterBootstrap(runtime);

	EXPECT_NE(runtime.lastModuleSource().find("struct RasterParams"), std::string::npos);
	EXPECT_EQ(runtime.lastLaunch().argumentCount, 1u);
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 1u);
}

TEST(RasterBootstrap, CpuReferenceSetsFrontFacingFlag)
{
	std::array<backend::RasterBootstrapVertex, 3> backFacingTriangle = {{
		backend::RasterBootstrapVertex{ 1.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 5.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 1.0f, 5.0f, 0.0f, 1.0f },
	}};
	std::array<backend::RasterBootstrapVertex, 3> frontFacingTriangle = {{
		backend::RasterBootstrapVertex{ 1.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 1.0f, 5.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 5.0f, 1.0f, 0.0f, 1.0f },
	}};

	backend::RasterBootstrapConfig config = {};
	config.width = 8u;
	config.height = 8u;

	auto back = backend::rasterBootstrapCpuReference(backFacingTriangle, config);
	auto front = backend::rasterBootstrapCpuReference(frontFacingTriangle, config);

	ASSERT_FALSE(back.invocations.empty());
	ASSERT_FALSE(front.invocations.empty());
	EXPECT_EQ(back.invocations.front().frontFacing, 0u);
	EXPECT_EQ(front.invocations.front().frontFacing, 1u);
}

TEST(RasterBootstrap, CpuReferenceProducesNoCoverageForDegenerateTriangle)
{
	std::array<backend::RasterBootstrapVertex, 3> triangle = {{
		backend::RasterBootstrapVertex{ 4.0f, 4.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 4.0f, 4.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 4.0f, 4.0f, 0.0f, 1.0f },
	}};

	backend::RasterBootstrapConfig config = {};
	config.width = 8u;
	config.height = 8u;

	backend::RasterBootstrapOutput output = backend::rasterBootstrapCpuReference(triangle, config);

	EXPECT_TRUE(output.valid);
	EXPECT_TRUE(output.invocations.empty());
}

#if SWIFTSHADER_GPU_USE_CUDA
TEST(RasterBootstrap, CudaRuntimeMatchesCpuReference)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::array<backend::RasterBootstrapVertex, 3> triangle = {{
		backend::RasterBootstrapVertex{ 1.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 5.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 1.0f, 5.0f, 0.0f, 1.0f },
	}};

	backend::RasterBootstrapConfig config = {};
	config.width = 8u;
	config.height = 8u;

	backend::RasterBootstrapOutput reference = backend::rasterBootstrapCpuReference(triangle, config);
	backend::RasterBootstrapOutput output = {};

	backend::CudaRuntimeAPI::resetGlobalCapture();
	ASSERT_TRUE(backend::runRasterBootstrap(runtime, triangle, config, &output));

	EXPECT_EQ(backend::CudaRuntimeAPI::globalLastLaunch().blockCountX, config.width);
	EXPECT_EQ(backend::CudaRuntimeAPI::globalLastLaunch().blockCountY, config.height);
	EXPECT_EQ(output.invocations.size(), reference.invocations.size());
	ASSERT_EQ(output.invocations.size(), reference.invocations.size());
	for(size_t i = 0; i < reference.invocations.size(); i++)
	{
		EXPECT_EQ(output.invocations[i].x, reference.invocations[i].x);
		EXPECT_EQ(output.invocations[i].y, reference.invocations[i].y);
		EXPECT_EQ(output.invocations[i].exportMask, reference.invocations[i].exportMask);
	}
}

TEST(RasterBootstrap, CudaRuntimeRejectsDegenerateTriangleLikeCpuReference)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::array<backend::RasterBootstrapVertex, 3> triangle = {{
		backend::RasterBootstrapVertex{ 4.0f, 4.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 4.0f, 4.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 4.0f, 4.0f, 0.0f, 1.0f },
	}};

	backend::RasterBootstrapConfig config = {};
	config.width = 8u;
	config.height = 8u;

	backend::RasterBootstrapOutput reference = backend::rasterBootstrapCpuReference(triangle, config);
	backend::RasterBootstrapOutput output = {};

	ASSERT_TRUE(reference.invocations.empty());
	ASSERT_TRUE(backend::runRasterBootstrap(runtime, triangle, config, &output));
	EXPECT_TRUE(output.invocations.empty());
}

TEST(RasterBootstrap, RasterFeedsFragmentBootstrap)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::array<backend::RasterBootstrapVertex, 3> triangle = {{
		backend::RasterBootstrapVertex{ 1.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 5.0f, 1.0f, 0.0f, 1.0f },
		backend::RasterBootstrapVertex{ 1.0f, 5.0f, 0.0f, 1.0f },
	}};

	backend::RasterBootstrapConfig rasterConfig = {};
	rasterConfig.width = 8u;
	rasterConfig.height = 8u;

	backend::FragmentBootstrapConfig fragmentConfig = {};
	fragmentConfig.colorR = 0.0f;
	fragmentConfig.colorG = 1.0f;
	fragmentConfig.colorB = 0.0f;
	fragmentConfig.colorA = 1.0f;

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runRasterFragmentBootstrap(runtime, triangle, rasterConfig, fragmentConfig, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 8u * 8u * 4u);

	size_t inside = ((1u * 8u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[inside + 0], 0u);
	EXPECT_EQ(colorBuffer[inside + 1], 255u);
	EXPECT_EQ(colorBuffer[inside + 2], 0u);
	EXPECT_EQ(colorBuffer[inside + 3], 255u);

	size_t outside = ((6u * 8u) + 6u) * 4u;
	EXPECT_EQ(colorBuffer[outside + 0], 0u);
	EXPECT_EQ(colorBuffer[outside + 1], 0u);
	EXPECT_EQ(colorBuffer[outside + 2], 0u);
	EXPECT_EQ(colorBuffer[outside + 3], 0u);
}
#endif
