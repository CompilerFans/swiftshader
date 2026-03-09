// Copyright 2021 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "DrawTester.hpp"
#include <filesystem>
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "Backend/CudaRuntimeAPI.hpp"
#	include <cstdlib>
#	include <fstream>
#	include <unistd.h>
#endif

#include "gmock/gmock.h"
#include "gtest/gtest.h"

class DrawTest : public testing::Test
{
};

namespace {

std::filesystem::path makeDrawArtifactPath(const char *name)
{
	auto dir = std::filesystem::current_path() / "draw-test-artifacts";
	std::filesystem::create_directories(dir);
	return dir / name;
}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
std::filesystem::path makeCudaLaunchStampPath(const char *suffix)
{
	return std::filesystem::temp_directory_path() / (std::string("swiftshader-cuda-launch-") + suffix + "-" + std::to_string(::getpid()) + ".log");
}

uint32_t countStampedLaunches(const std::filesystem::path &path)
{
	std::ifstream stream(path);
	uint32_t count = 0;
	std::string line;
	while(std::getline(stream, line))
	{
		if(!line.empty())
		{
			count++;
		}
	}
	return count;
}

std::string readTextFile(const std::filesystem::path &path)
{
	std::ifstream stream(path);
	return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

#endif
}  // namespace

// Test that a vertex shader with no gl_Position works.
// This was fixed in swiftshader-cl/51808
TEST_F(DrawTest, VertexShaderNoPositionOutput)
{
	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
		};

		Vertex vertexBufferData[] = {
			{ { 1.0f, 1.0f, 0.5f } },
			{ { -1.0f, 1.0f, 0.5f } },
			{ { 0.0f, -1.0f, 0.5f } }
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;

			void main()
			{
				// Remove gl_Position on purpose for the test
				//gl_Position = vec4(inPos.xyz, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(1.0, 1.0, 1.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
}

TEST_F(DrawTest, VertexShaderAppliesOffsetFromGlsl)
{
	auto artifactPath = makeDrawArtifactPath("vertex-offset-glsl.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-offset-glsl");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
		};

		Vertex vertexBufferData[] = {
			{ { 1.0f, 1.0f, 0.5f } },
			{ { -1.0f, 1.0f, 0.5f } },
			{ { 0.0f, -1.0f, 0.5f } }
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;

			void main()
			{
				gl_Position = vec4(inPos.x - 0.5, inPos.y, inPos.z, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(1.0, 0.0, 0.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto pixel = tester.readbackPixel(320, 240);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, VertexShaderAppliesOffsetFromSpirvModule)
{
	auto artifactPath = makeDrawArtifactPath("vertex-offset-spirv.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-offset-spirv");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[2];
		};

		Vertex vertexBufferData[] = {
			{ { 1.0f, 1.0f } },
			{ { -1.0f, 1.0f } },
			{ { 0.0f, -1.0f } }
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec2 inPos;

			void main()
			{
				gl_Position = vec4(inPos.x + 0.5, inPos.y, 0.0, 1.0);
			})";

		auto spirv = Util::compileGLSLtoSPIRV(vertexShader, EShLanguage::EShLangVertex);
		return tester.createShaderModule(spirv);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(1.0, 0.0, 0.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto pixel = tester.readbackPixel(960, 240);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, FragmentShaderUsesFragCoordQuadrantColors)
{
	auto artifactPath = makeDrawArtifactPath("fragcoord-quadrant-colors.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("fragcoord-quadrant");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[2];
		};

		Vertex vertexBufferData[] = {
			{ { -1.0f, -1.0f } },
			{ { 3.0f, -1.0f } },
			{ { -1.0f, 3.0f } }
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec2 inPos;

			void main()
			{
				gl_Position = vec4(inPos, 0.0, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				bool right = gl_FragCoord.x >= 640.0;
				bool bottom = gl_FragCoord.y >= 360.0;

				if(!right && !bottom)
				{
					outColor = vec4(1.0, 0.0, 0.0, 1.0);
				}
				else if(right && !bottom)
				{
					outColor = vec4(0.0, 1.0, 0.0, 1.0);
				}
				else if(!right && bottom)
				{
					outColor = vec4(0.0, 0.0, 1.0, 1.0);
				}
				else
				{
					outColor = vec4(1.0, 1.0, 0.0, 1.0);
				}
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto topLeft = tester.readbackPixel(320, 180);
	auto topRight = tester.readbackPixel(960, 180);
	auto bottomLeft = tester.readbackPixel(320, 540);
	auto bottomRight = tester.readbackPixel(960, 540);

	EXPECT_GT(topLeft[0], 200);
	EXPECT_LT(topLeft[1], 80);
	EXPECT_LT(topLeft[2], 80);

	EXPECT_LT(topRight[0], 80);
	EXPECT_GT(topRight[1], 200);
	EXPECT_LT(topRight[2], 80);

	EXPECT_LT(bottomLeft[0], 80);
	EXPECT_LT(bottomLeft[1], 80);
	EXPECT_GT(bottomLeft[2], 200);

	EXPECT_GT(bottomRight[0], 200);
	EXPECT_GT(bottomRight[1], 200);
	EXPECT_LT(bottomRight[2], 80);

	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, FragmentShaderUsesFragCoordQuadrantColorsInsideTriangle)
{
	auto artifactPath = makeDrawArtifactPath("fragcoord-quadrant-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("fragcoord-quadrant-triangle");
	auto sourceDumpPath = makeCudaLaunchStampPath("fragcoord-quadrant-triangle-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.95f } },
			{ { 0.95f, -0.95f } },
			{ { 0.0f, 0.95f } }
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec2 inPos;

			void main()
			{
				gl_Position = vec4(inPos, 0.0, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				bool right = gl_FragCoord.x >= 640.0;
				bool bottom = gl_FragCoord.y >= 360.0;

				if(!right && !bottom)
				{
					outColor = vec4(1.0, 0.0, 0.0, 1.0);
				}
				else if(right && !bottom)
				{
					outColor = vec4(0.0, 1.0, 0.0, 1.0);
				}
				else if(!right && bottom)
				{
					outColor = vec4(0.0, 0.0, 1.0, 1.0);
				}
				else
				{
					outColor = vec4(1.0, 1.0, 0.0, 1.0);
				}
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto topLeft = tester.readbackPixel(600, 180);
	auto topRight = tester.readbackPixel(680, 180);
	auto bottomLeft = tester.readbackPixel(560, 540);
	auto bottomRight = tester.readbackPixel(720, 540);

	EXPECT_GT(topLeft[0], 200);
	EXPECT_LT(topLeft[1], 80);
	EXPECT_LT(topLeft[2], 80);

	EXPECT_LT(topRight[0], 80);
	EXPECT_GT(topRight[1], 200);
	EXPECT_LT(topRight[2], 80);

	EXPECT_LT(bottomLeft[0], 80);
	EXPECT_LT(bottomLeft[1], 80);
	EXPECT_GT(bottomLeft[2], 200);

	EXPECT_GT(bottomRight[0], 200);
	EXPECT_GT(bottomRight[1], 200);
	EXPECT_LT(bottomRight[2], 80);

	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("bool left = invocation.x * 2u < params.width"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, SolidColorTriangle)
{
	auto artifactPath = makeDrawArtifactPath("solid-color-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("draw");
	auto sourceDumpPath = makeCudaLaunchStampPath("draw-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	backend::CudaRuntimeAPI::resetGlobalCapture();
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif
	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
		};

		Vertex vertexBufferData[] = {
			{ { 1.0f, 1.0f, 0.5f } },
			{ { -1.0f, 1.0f, 0.5f } },
			{ { 0.0f, -1.0f, 0.5f } }
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;

			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(1.0, 0.0, 0.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 240);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_GT(pixel[3], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("outR = packColor(1.0f);"), std::string::npos);
	EXPECT_NE(sourceDump.find("outG = packColor(0.0f);"), std::string::npos);
	EXPECT_NE(sourceDump.find("outB = packColor(0.0f);"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, MultipleSolidColorTriangles)
{
	auto artifactPath = makeDrawArtifactPath("multiple-solid-color-triangles.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("multi-draw");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.90f, 0.80f, 0.50f } }, { { -0.50f, 0.80f, 0.50f } }, { { -0.70f, 0.40f, 0.50f } },
			{ { -0.20f, 0.70f, 0.50f } }, { { 0.20f, 0.70f, 0.50f } }, { { 0.00f, 0.30f, 0.50f } },
			{ { 0.50f, 0.75f, 0.50f } }, { { 0.90f, 0.75f, 0.50f } }, { { 0.70f, 0.35f, 0.50f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;

			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(1.0, 0.0, 0.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.draw(3, 1, 3, 0);
		commandBuffer.draw(3, 1, 6, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto leftPixel = tester.readbackPixel(192, 576);
	auto centerPixel = tester.readbackPixel(640, 540);
	auto rightPixel = tester.readbackPixel(1088, 558);

	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);

	EXPECT_GT(centerPixel[0], 200);
	EXPECT_LT(centerPixel[1], 80);
	EXPECT_LT(centerPixel[2], 80);

	EXPECT_GT(rightPixel[0], 200);
	EXPECT_LT(rightPixel[1], 80);
	EXPECT_LT(rightPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, VertexColorTriangleInterpolation)
{
	auto artifactPath = makeDrawArtifactPath("vertex-color-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-color-triangle");
	auto sourceDumpPath = makeCudaLaunchStampPath("vertex-color-triangle-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			layout(location = 0) out vec3 vColor;

			void main()
			{
				gl_Position = vec4(inPos, 1.0);
				vColor = inColor;
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) in vec3 vColor;
			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(vColor, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto topPixel = tester.readbackPixel(640, 180);
	EXPECT_GT(topPixel[3], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("invocation.barycentric0"), std::string::npos);
	EXPECT_NE(sourceDump.find("params.vertexColor0R"), std::string::npos);
	EXPECT_NE(sourceDump.find("params.vertexColor1G"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, DynamicVertexBufferUpdateChangesRenderedColor)
{
	struct Vertex
	{
		float position[3];
		float color[3];
	};

	Vertex initialVertices[] = {
		{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		{ { 0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		{ { 0.0f, 0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
	};

	Vertex updatedVertices[] = {
		{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.0f, 0.95f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
	};

	DrawTester tester;
	tester.onCreateVertexBuffers([&](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));

		tester.addVertexBuffer(initialVertices, sizeof(initialVertices), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			layout(location = 0) out vec3 vColor;

			void main()
			{
				gl_Position = vec4(inPos, 1.0);
				vColor = inColor;
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) in vec3 vColor;
			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(vColor, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	auto firstPixel = tester.readbackPixel(640, 180);

	tester.updateVertexBufferData(updatedVertices, sizeof(updatedVertices));
	tester.renderFrame();
	auto secondPixel = tester.readbackPixel(640, 180);

	EXPECT_GT(firstPixel[3], 200);
	EXPECT_GT(secondPixel[3], 200);
	EXPECT_GT(firstPixel[2], firstPixel[0]);
	EXPECT_GT(secondPixel[0], secondPixel[2]);
}

TEST_F(DrawTest, IndexedVertexColorTriangleInterpolation)
{
	struct Vertex
	{
		float position[3];
		float color[3];
	};

	Vertex vertexBufferData[] = {
		{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
		{ { 0.95f, 0.85f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 1.0f, 0.0f } },
	};
	uint16_t indexBufferData[] = { 0u, 2u, 3u };

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-vertex-color");
	auto sourceDumpPath = makeCudaLaunchStampPath("indexed-vertex-color-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([&](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			layout(location = 0) out vec3 vColor;

			void main()
			{
				gl_Position = vec4(inPos, 1.0);
				vColor = inColor;
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) in vec3 vColor;
			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(vColor, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(3, 1, 0, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	auto pixel = tester.readbackPixel(960, 520);
	EXPECT_GT(pixel[3], 200);

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("invocation.barycentric0"), std::string::npos);
	EXPECT_NE(sourceDump.find("params.vertexColor0R"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, FragmentShaderUsesFrontFacingColors)
{
	auto artifactPath = makeDrawArtifactPath("front-facing-colors.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("front-facing-colors");
	auto sourceDumpPath = makeCudaLaunchStampPath("front-facing-colors-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.45f, 0.75f, 0.5f } },
			{ { -0.05f, -0.85f, 0.5f } },
			{ { 0.05f, -0.85f, 0.5f } },
			{ { 0.95f, -0.85f, 0.5f } },
			{ { 0.45f, 0.75f, 0.5f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;

			void main()
			{
				gl_Position = vec4(inPos, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = gl_FrontFacing ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(6, 1, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto leftPixel = tester.readbackPixel(320, 360);
	auto rightPixel = tester.readbackPixel(960, 360);
	EXPECT_GT(leftPixel[0], leftPixel[2]);
	EXPECT_GT(rightPixel[2], rightPixel[0]);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("invocation.frontFacing"), std::string::npos);
	EXPECT_NE(sourceDump.find("params.backColorR"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, FragmentShaderDiscardsLeftHalfByFragCoord)
{
	auto artifactPath = makeDrawArtifactPath("fragcoord-discard-left.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("fragcoord-discard-left");
	auto sourceDumpPath = makeCudaLaunchStampPath("fragcoord-discard-left-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[2]; };
		Vertex vertexBufferData[] = {
			{ { -1.0f, -1.0f } },
			{ { 3.0f, -1.0f } },
			{ { -1.0f, 3.0f } }
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec2 inPos;
			void main() { gl_Position = vec4(inPos, 0.0, 1.0); })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main()
			{
				if(gl_FragCoord.x < 640.0) discard;
				outColor = vec4(1.0, 0.0, 0.0, 1.0);
			})";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto leftPixel = tester.readbackPixel(320, 180);
	auto rightPixel = tester.readbackPixel(960, 180);
	EXPECT_LT(leftPixel[0], 180);
	EXPECT_GT(rightPixel[0], 200);
	EXPECT_LT(rightPixel[1], 80);
	EXPECT_LT(rightPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("invocation.x * 2u < params.width"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, FragmentShaderUsesFragDepthFromInterpolatedColor)
{
	auto artifactPath = makeDrawArtifactPath("fragdepth-interpolated-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("fragdepth-interpolated-color");
	auto sourceDumpPath = makeCudaLaunchStampPath("fragdepth-interpolated-color-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableDepthTest(true, true, vk::CompareOp::eLess);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.75f, -0.75f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
			{ { 0.0f, 0.75f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
			{ { 0.75f, -0.75f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
			{ { -0.75f, -0.75f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.0f, 0.75f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.75f, -0.75f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			layout(location = 0) out vec3 vColor;
			void main() { gl_Position = vec4(inPos, 1.0); vColor = inColor; })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec3 vColor;
			layout(location = 0) out vec4 outColor;
			void main()
			{
				outColor = vec4(vColor, 1.0);
				gl_FragDepth = vColor.b > vColor.r ? 0.2 : 0.8;
			})";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.draw(3, 1, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto pixel = tester.readbackPixel(640, 360);
	EXPECT_GT(pixel[2], pixel[0]);
	EXPECT_GT(pixel[3], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("params.nearDepth"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, PointListConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("point-list-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("point-list-constant-color");
	auto sourceDumpPath = makeCudaLaunchStampPath("point-list-constant-color-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::ePointList);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = { { { 0.0f, 0.0f, 0.5f } } };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			void main()
			{
				gl_Position = vec4(inPos, 1.0);
				gl_PointSize = 64.0;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 360);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, FragmentShaderUsesPointCoordGradient)
{
	auto artifactPath = makeDrawArtifactPath("pointcoord-gradient.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("pointcoord-gradient");
	auto sourceDumpPath = makeCudaLaunchStampPath("pointcoord-gradient-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::ePointList);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = { { { 0.0f, 0.0f, 0.5f } } };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			void main()
			{
				gl_Position = vec4(inPos, 1.0);
				gl_PointSize = 64.0;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(gl_PointCoord.xy, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto leftPixel = tester.readbackPixel(624, 360);
	auto rightPixel = tester.readbackPixel(656, 360);
	EXPECT_LT(leftPixel[0], rightPixel[0]);
	EXPECT_GT(leftPixel[1], 0);
	EXPECT_GT(rightPixel[1], 0);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("invocation.pointCoordX"), std::string::npos);
	EXPECT_NE(sourceDump.find("invocation.pointCoordY"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, FragmentShaderUsesFlatInterpolatedColor)
{
	auto artifactPath = makeDrawArtifactPath("flat-interpolated-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("flat-interpolated-color");
	auto sourceDumpPath = makeCudaLaunchStampPath("flat-interpolated-color-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ { 0.0f, 0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			flat layout(location = 0) out vec3 vColor;
			void main() { gl_Position = vec4(inPos, 1.0); vColor = inColor; })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			flat layout(location = 0) in vec3 vColor;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 360);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("params.vertexColor0R"), std::string::npos);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, TriangleStripConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("triangle-strip-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("triangle-strip-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.8f, -0.8f, 0.5f } },
			{ { -0.2f, 0.8f, 0.5f } },
			{ { 0.2f, -0.8f, 0.5f } },
			{ { 0.8f, 0.8f, 0.5f } },
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			void main() { gl_Position = vec4(inPos, 1.0); })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 360);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}
