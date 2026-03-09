#include "Backend/FakeRuntimeAPI.hpp"
#include "Backend/FragmentBootstrap.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#endif

#include <gtest/gtest.h>

TEST(FragmentBootstrap, EmitsFragmentStageWrapperAndShaderBody)
{
	std::string source = backend::fragmentBootstrapCudaSource();

	EXPECT_NE(source.find("struct FragmentInvocation"), std::string::npos);
	EXPECT_NE(source.find("struct FsParams"), std::string::npos);
	EXPECT_NE(source.find("static __device__ void fs_main"), std::string::npos);
	EXPECT_NE(source.find("extern \"C\" __global__ void fs_entry"), std::string::npos);
	EXPECT_NE(source.find("params.colorBuffer[offset + 0]"), std::string::npos);
}

TEST(FragmentBootstrap, EmitsFragCoordQuadrantShaderWhenRequested)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FragCoordQuadrants;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("bool left = invocation.x * 2u < params.width"), std::string::npos);
	EXPECT_NE(source.find("bool top = invocation.y * 2u < params.height"), std::string::npos);
	EXPECT_NE(source.find("outR = left && top ? 255u :"), std::string::npos);
}

TEST(FragmentBootstrap, EmitsPointCoordGradientShaderWhenRequested)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::PointCoordGradient;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("invocation.pointCoordX"), std::string::npos);
	EXPECT_NE(source.find("invocation.pointCoordY"), std::string::npos);
}

TEST(FragmentBootstrap, EmitsFlatInterpolatedColorShaderWhenRequested)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FlatInterpolatedColor;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("params.vertexColor0R"), std::string::npos);
	EXPECT_EQ(source.find("invocation.barycentric0"), std::string::npos);
}

TEST(FragmentBootstrap, EmitsInterpolatedColorShaderFromBarycentrics)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::InterpolatedColor;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("invocation.barycentric0"), std::string::npos);
	EXPECT_NE(source.find("params.vertexColor0R"), std::string::npos);
	EXPECT_NE(source.find("params.vertexColor1G"), std::string::npos);
	EXPECT_NE(source.find("params.vertexColor2B"), std::string::npos);
	EXPECT_EQ(source.find("outR = packColor(invocation.colorR);"), std::string::npos);
}

TEST(FragmentBootstrap, EmitsInterpolatedColorFragDepthShaderWhenRequested)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("params.nearDepth"), std::string::npos);
	EXPECT_NE(source.find("outDepth = colorB > colorR ? params.nearDepth : params.farDepth;"), std::string::npos);
}

TEST(FragmentBootstrap, EmitsFrontFacingBinaryShaderWhenRequested)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FrontFacingBinaryColors;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("invocation.frontFacing"), std::string::npos);
	EXPECT_NE(source.find("params.backColorR"), std::string::npos);
	EXPECT_NE(source.find("params.colorR"), std::string::npos);
}


TEST(FragmentBootstrap, EmitsFragCoordDiscardLeftShaderWhenRequested)
{
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FragCoordDiscardLeftConstantColor;

	std::string source = backend::fragmentBootstrapCudaSource(config);

	EXPECT_NE(source.find("invocation.x * 2u < params.width"), std::string::npos);
	EXPECT_NE(source.find("outA = 0u;"), std::string::npos);
}

TEST(FragmentBootstrap, LaunchUsesSingleFsParamsArgument)
{
	backend::FakeRuntimeAPI runtime;
	backend::launchFragmentBootstrap(runtime);

	EXPECT_NE(runtime.lastModuleSource().find("struct FsParams"), std::string::npos);
	EXPECT_EQ(runtime.lastLaunch().argumentCount, 1u);
	EXPECT_EQ(runtime.lastLaunch().groupCountX, 1u);
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
TEST(FragmentBootstrap, CudaRuntimeWritesConstantColor)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations = {
		{ 1u, 1u, 1u, 0u },
		{ 2u, 1u, 1u, 0u },
	};
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 4u, 4u, invocations, backend::FragmentBootstrapConfig{}, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 4u * 4u * 4u);

	size_t first = ((1u * 4u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[first + 0], 255u);
	EXPECT_EQ(colorBuffer[first + 1], 0u);
	EXPECT_EQ(colorBuffer[first + 2], 0u);
	EXPECT_EQ(colorBuffer[first + 3], 255u);
}

TEST(FragmentBootstrap, CudaRuntimeSkipsHelperAndExportMaskedInvocations)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations = {
		{ 1u, 1u, 0u, 0u },
		{ 2u, 1u, 1u, 1u },
		{ 3u, 1u, 1u, 0u },
	};
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 4u, 4u, invocations, backend::FragmentBootstrapConfig{}, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 4u * 4u * 4u);

	size_t skippedExport = ((1u * 4u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[skippedExport + 0], 0u);
	EXPECT_EQ(colorBuffer[skippedExport + 3], 0u);

	size_t skippedHelper = ((1u * 4u) + 2u) * 4u;
	EXPECT_EQ(colorBuffer[skippedHelper + 0], 0u);
	EXPECT_EQ(colorBuffer[skippedHelper + 3], 0u);

	size_t written = ((1u * 4u) + 3u) * 4u;
	EXPECT_EQ(colorBuffer[written + 0], 255u);
	EXPECT_EQ(colorBuffer[written + 3], 255u);
}

TEST(FragmentBootstrap, CudaRuntimeWritesFragCoordQuadrantColors)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations = {
		{ 1u, 1u, 1u, 0u },
		{ 6u, 1u, 1u, 0u },
		{ 1u, 6u, 1u, 0u },
		{ 6u, 6u, 1u, 0u },
	};
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FragCoordQuadrants;
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 8u, 8u, invocations, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 8u * 8u * 4u);

	size_t topLeft = ((1u * 8u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[topLeft + 0], 255u);
	EXPECT_EQ(colorBuffer[topLeft + 1], 0u);
	EXPECT_EQ(colorBuffer[topLeft + 2], 0u);
	EXPECT_EQ(colorBuffer[topLeft + 3], 255u);

	size_t topRight = ((1u * 8u) + 6u) * 4u;
	EXPECT_EQ(colorBuffer[topRight + 0], 0u);
	EXPECT_EQ(colorBuffer[topRight + 1], 255u);
	EXPECT_EQ(colorBuffer[topRight + 2], 0u);
	EXPECT_EQ(colorBuffer[topRight + 3], 255u);

	size_t bottomLeft = ((6u * 8u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[bottomLeft + 0], 0u);
	EXPECT_EQ(colorBuffer[bottomLeft + 1], 0u);
	EXPECT_EQ(colorBuffer[bottomLeft + 2], 255u);
	EXPECT_EQ(colorBuffer[bottomLeft + 3], 255u);

	size_t bottomRight = ((6u * 8u) + 6u) * 4u;
	EXPECT_EQ(colorBuffer[bottomRight + 0], 255u);
	EXPECT_EQ(colorBuffer[bottomRight + 1], 255u);
	EXPECT_EQ(colorBuffer[bottomRight + 2], 0u);
	EXPECT_EQ(colorBuffer[bottomRight + 3], 255u);
}


TEST(FragmentBootstrap, CudaRuntimeDiscardsLeftHalfForFragCoordMode)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations = {
		{ 1u, 1u, 1u, 0u, 1u },
		{ 6u, 1u, 1u, 0u, 1u },
	};
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FragCoordDiscardLeftConstantColor;
	config.colorR = 1.0f;
	config.colorG = 0.0f;
	config.colorB = 0.0f;
	config.colorA = 1.0f;
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 8u, 8u, invocations, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 8u * 8u * 4u);

	size_t left = ((1u * 8u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[left + 0], 0u);
	EXPECT_EQ(colorBuffer[left + 3], 0u);

	size_t right = ((1u * 8u) + 6u) * 4u;
	EXPECT_EQ(colorBuffer[right + 0], 255u);
	EXPECT_EQ(colorBuffer[right + 1], 0u);
	EXPECT_EQ(colorBuffer[right + 2], 0u);
	EXPECT_EQ(colorBuffer[right + 3], 255u);
}

TEST(FragmentBootstrap, CudaRuntimeWritesFrontFacingBinaryColors)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	std::vector<backend::FragmentBootstrapInvocation> invocations(2);
	invocations[0].x = 1u;
	invocations[0].y = 1u;
	invocations[0].exportMask = 1u;
	invocations[0].frontFacing = 1u;
	invocations[1].x = 2u;
	invocations[1].y = 1u;
	invocations[1].exportMask = 1u;
	invocations[1].frontFacing = 0u;
	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::FrontFacingBinaryColors;
	config.colorR = 1.0f;
	config.colorG = 0.0f;
	config.colorB = 0.0f;
	config.colorA = 1.0f;
	config.backColorR = 0.0f;
	config.backColorG = 0.0f;
	config.backColorB = 1.0f;
	config.backColorA = 1.0f;
	std::vector<uint8_t> colorBuffer;

	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 4u, 4u, invocations, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 4u * 4u * 4u);

	size_t front = ((1u * 4u) + 1u) * 4u;
	EXPECT_EQ(colorBuffer[front + 0], 255u);
	EXPECT_EQ(colorBuffer[front + 1], 0u);
	EXPECT_EQ(colorBuffer[front + 2], 0u);

	size_t back = ((1u * 4u) + 2u) * 4u;
	EXPECT_EQ(colorBuffer[back + 0], 0u);
	EXPECT_EQ(colorBuffer[back + 1], 0u);
	EXPECT_EQ(colorBuffer[back + 2], 255u);
}
#endif


TEST(FragmentBootstrap, CudaRuntimeSamplesTexture2DNearest)
{
	backend::CudaRuntimeAPI runtime;
	ASSERT_TRUE(runtime.isAvailable()) << runtime.initializationError();

	backend::FragmentBootstrapConfig config = {};
	config.shaderKind = backend::FragmentBootstrapShaderKind::Texture2DColor;
	config.vertexTexCoord0U = 0.0f;
	config.vertexTexCoord0V = 0.0f;
	config.vertexTexCoord1U = 1.0f;
	config.vertexTexCoord1V = 0.0f;
	config.vertexTexCoord2U = 0.0f;
	config.vertexTexCoord2V = 1.0f;
	config.textureWidth = 2u;
	config.textureHeight = 2u;
	config.textureRowPitchTexels = 2u;
	config.textureFilterLinear = 0u;
	config.textureAddressModeU = 0u;
	config.textureAddressModeV = 0u;
	config.textureData = {
		255u, 0u,   0u,   255u,
		0u,   255u, 0u,   255u,
		0u,   0u,   255u, 255u,
		255u, 255u, 0u,   255u,
	};

	std::vector<backend::FragmentBootstrapInvocation> invocations(1);
	invocations[0].x = 0u;
	invocations[0].y = 0u;
	invocations[0].barycentric0 = 1.0f;
	invocations[0].barycentric1 = 0.0f;
	invocations[0].barycentric2 = 0.0f;

	std::vector<uint8_t> colorBuffer;
	ASSERT_TRUE(backend::runFragmentBootstrap(runtime, 1u, 1u, invocations, config, &colorBuffer));
	ASSERT_EQ(colorBuffer.size(), 4u);
	EXPECT_EQ(colorBuffer[0], 255u);
	EXPECT_EQ(colorBuffer[1], 0u);
	EXPECT_EQ(colorBuffer[2], 0u);
	EXPECT_EQ(colorBuffer[3], 255u);
}
