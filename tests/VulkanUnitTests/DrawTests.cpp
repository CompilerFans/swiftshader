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
#	include <fstream>
#	include <unistd.h>
#endif

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <cstdlib>

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

void configureSolidColorTriangleDraw(DrawTester &tester)
{
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
}
}  // namespace

TEST_F(DrawTest, ConstructThenDestroyWithoutInitialize)
{
	DrawTester tester;
}

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
	configureSolidColorTriangleDraw(tester);

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

TEST_F(DrawTest, InitializeThenDestroyWithoutRender)
{
	DrawTester tester;
	configureSolidColorTriangleDraw(tester);
	tester.initialize();
}

TEST_F(DrawTest, RenderWithoutPresentThenDestroy)
{
	DrawTester tester;
	configureSolidColorTriangleDraw(tester);
	tester.initialize();
	tester.renderFrameWithoutPresent();
}

TEST_F(DrawTest, DynamicRenderingSolidColorTriangle)
{
	auto artifactPath = makeDrawArtifactPath("dynamic-rendering-solid-color-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("dynamic-rendering-solid-color-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableDynamicRendering();
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
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
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
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

TEST_F(DrawTest, FragmentShaderUsesNoPerspectiveColor)
{
	auto artifactPath = makeDrawArtifactPath("noperspective-color-triangle.bmp");
	std::filesystem::remove(artifactPath);

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.0f,  0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));

		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 450
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			layout(location = 0) out vec3 smoothColor;
			layout(location = 1) noperspective out vec3 noPerspectiveColor;

			void main()
			{
				float w = 1.0 + 0.5 * inColor.r;
				gl_Position = vec4(inPos.xy * w, inPos.z, w);
				smoothColor = inColor;
				noPerspectiveColor = inColor;
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 450
			precision highp float;

			layout(location = 0) in vec3 smoothColor;
			layout(location = 1) noperspective in vec3 noPerspectiveColor;
			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = vec4(noPerspectiveColor.r, smoothColor.r, 0.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	int maxDiff = 0;
	int sampledTrianglePixels = 0;
	const int sampleXs[] = { 160, 320, 480, 640, 800, 960, 1120 };
	const int sampleYs[] = { 120, 240, 360, 480, 600 };
	for(int y : sampleYs)
	{
		for(int x : sampleXs)
		{
			auto pixel = tester.readbackPixel(x, y);
			if(pixel[2] < 80)  // Triangle draws with B = 0 over a gray (B ~ 128) cleared background.
			{
				sampledTrianglePixels++;
				maxDiff = std::max(maxDiff, std::abs(int(pixel[0]) - int(pixel[1])));
			}
		}
	}

	EXPECT_GT(sampledTrianglePixels, 0);
	EXPECT_GT(maxDiff, 3);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
}
TEST_F(DrawTest, VertexInputDynamicStateVertexColorTriangleInterpolation)
{
	auto artifactPath = makeDrawArtifactPath("vertex-input-dynamic-state-color-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-input-dynamic-state-color-triangle");
	auto sourceDumpPath = makeCudaLaunchStampPath("vertex-input-dynamic-state-color-triangle-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableVertexInputDynamicState();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.0f,  0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
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
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
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
	EXPECT_NEAR(leftPixel[0], 128, 8);
	EXPECT_NEAR(leftPixel[1], 128, 8);
	EXPECT_NEAR(leftPixel[2], 128, 8);
	EXPECT_GT(leftPixel[3], 200);
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


TEST_F(DrawTest, TriangleFanConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("triangle-fan-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("triangle-fan-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleFan);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { 0.0f, 0.8f, 0.5f } },
			{ { -0.8f, -0.8f, 0.5f } },
			{ { 0.0f, -0.2f, 0.5f } },
			{ { 0.8f, -0.8f, 0.5f } },
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
	auto pixel = tester.readbackPixel(640, 420);
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





TEST_F(DrawTest, LineListConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("line-list-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("line-list-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eLineList);
	tester.setLineWidth(32.0f);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.8f, 0.0f, 0.5f } },
			{ { 0.8f, 0.0f, 0.5f } },
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


TEST_F(DrawTest, LineStripConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("line-strip-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("line-strip-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eLineStrip);
	tester.setLineWidth(32.0f);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.8f, -0.4f, 0.5f } },
			{ { 0.0f, 0.4f, 0.5f } },
			{ { 0.8f, -0.4f, 0.5f } },
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
	auto frame = tester.readbackFrameRgba();
	tester.saveFrame(artifactPath);
	size_t redPixelCount = 0;
	for(size_t offset = 0; offset + 3 < frame.size(); offset += 4)
	{
		if(frame[offset + 0] > 200 && frame[offset + 1] < 80 && frame[offset + 2] < 80 && frame[offset + 3] > 200)
		{
			redPixelCount++;
		}
	}
	EXPECT_GT(redPixelCount, 1000u);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, PointListUsesVertexPointSize)
{
	auto artifactPath = makeDrawArtifactPath("point-list-vertex-point-size.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("point-list-vertex-point-size");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
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
				gl_PointSize = 16.0;
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
	auto frame = tester.readbackFrameRgba();
	tester.saveFrame(artifactPath);
	size_t redPixelCount = 0;
	for(size_t offset = 0; offset + 3 < frame.size(); offset += 4)
	{
		if(frame[offset + 0] > 200 && frame[offset + 1] < 80 && frame[offset + 2] < 80 && frame[offset + 3] > 200)
		{
			redPixelCount++;
		}
	}
	EXPECT_GT(redPixelCount, 100u);
	EXPECT_LT(redPixelCount, 800u);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, IndexedLineStripConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("indexed-line-strip-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-line-strip-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eLineStrip);
	tester.setLineWidth(32.0f);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.8f, -0.4f, 0.5f } },
			{ { 0.0f, 0.4f, 0.5f } },
			{ { 0.8f, -0.4f, 0.5f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(3, 1, 0, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	auto frame = tester.readbackFrameRgba();
	tester.saveFrame(artifactPath);
	size_t redPixelCount = 0;
	for(size_t offset = 0; offset + 3 < frame.size(); offset += 4)
	{
		if(frame[offset + 0] > 200 && frame[offset + 1] < 80 && frame[offset + 2] < 80 && frame[offset + 3] > 200)
		{
			redPixelCount++;
		}
	}
	EXPECT_GT(redPixelCount, 1000u);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, IndexedTriangleFanConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("indexed-triangle-fan-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-triangle-fan-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleFan);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { 0.0f, 0.8f, 0.5f } },
			{ { -0.8f, -0.8f, 0.5f } },
			{ { 0.0f, -0.2f, 0.5f } },
			{ { 0.8f, -0.8f, 0.5f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u, 3u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(4, 1, 0, 0, 0);
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


TEST_F(DrawTest, IndexedPointListConstantColor)
{
	auto artifactPath = makeDrawArtifactPath("indexed-point-list-constant-color.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-point-list-constant-color");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::ePointList);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = { { { 0.0f, 0.0f, 0.5f } } };
		uint16_t indexBufferData[] = { 0u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(1, 1, 0, 0, 0);
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


TEST_F(DrawTest, IndexedTriangleStripWithPrimitiveRestart)
{
	auto artifactPath = makeDrawArtifactPath("indexed-triangle-strip-primitive-restart.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-triangle-strip-primitive-restart");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip);
	tester.setPrimitiveRestartEnable(true);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.55f, 0.25f, 0.5f } },
			{ { -0.15f, -0.85f, 0.5f } },
			{ { 0.15f, -0.85f, 0.5f } },
			{ { 0.55f, 0.25f, 0.5f } },
			{ { 0.95f, -0.85f, 0.5f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u, 0xFFFFu, 3u, 4u, 5u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(7, 1, 0, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(320, 360);
	auto rightPixel = tester.readbackPixel(960, 360);
	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);
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


TEST_F(DrawTest, IndexedPointCoordGradient)
{
	auto artifactPath = makeDrawArtifactPath("indexed-pointcoord-gradient.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-pointcoord-gradient");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.setPrimitiveTopology(vk::PrimitiveTopology::ePointList);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = { { { 0.0f, 0.0f, 0.5f } } };
		uint16_t indexBufferData[] = { 0u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(1, 1, 0, 0, 0);
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
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, TexturedTriangleNearest)
{
	auto artifactPath = makeDrawArtifactPath("textured-triangle-nearest.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("textured-triangle-nearest");
	auto sourceDumpPath = makeCudaLaunchStampPath("textured-triangle-nearest-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f,  0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;
			void main() { outColor = texture(texSampler, inTexCoord); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &tester) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});

	tester.onUpdateDescriptorSet([](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm).obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint32_t *>(buffer.mapMemory());
		data[0] = 0xFF0000FFu; // red in little-endian BGRA-backed host copy path
		data[1] = 0xFF00FF00u; // green
		data[2] = 0xFFFF0000u; // blue
		data[3] = 0xFF00FFFFu; // yellow
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eNearest;
		samplerInfo.minFilter = vk::Filter::eNearest;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
		auto sampler = tester.addSampler(samplerInfo);
		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();
		imageInfo.sampler = sampler.obj;
		std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 1;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &imageInfo;
		device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(480, 420);
	EXPECT_GT(pixel[3], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("textureData"), std::string::npos);
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, TexturedTriangleSeparateImageSamplerNearest)
{
	auto artifactPath = makeDrawArtifactPath("textured-triangle-separate-image-sampler-nearest.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("textured-triangle-separate-image-sampler-nearest");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f,  0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 450
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 450
			precision highp float;
			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(set = 0, binding = 0) uniform texture2D tex;
			layout(set = 0, binding = 1) uniform sampler texSampler;
			void main()
			{
				outColor = texture(sampler2D(tex, texSampler), inTexCoord);
			})";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding imageBinding;
		imageBinding.binding = 0;
		imageBinding.descriptorCount = 1;
		imageBinding.descriptorType = vk::DescriptorType::eSampledImage;
		imageBinding.pImmutableSamplers = nullptr;
		imageBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding samplerBinding;
		samplerBinding.binding = 1;
		samplerBinding.descriptorCount = 1;
		samplerBinding.descriptorType = vk::DescriptorType::eSampler;
		samplerBinding.pImmutableSamplers = nullptr;
		samplerBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		return { imageBinding, samplerBinding };
	});

	tester.onUpdateDescriptorSet([](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
		                                vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
		                    .obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint8_t *>(buffer.mapMemory());
		for(int i = 0; i < 16; i += 4)
		{
			data[i + 0] = 255u;
			data[i + 1] = 0u;
			data[i + 2] = 0u;
			data[i + 3] = 255u;
		}
		buffer.unmapMemory();

		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eNearest;
		samplerInfo.minFilter = vk::Filter::eNearest;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
		auto sampler = tester.addSampler(samplerInfo);

		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();

		vk::DescriptorImageInfo samplerDescriptorInfo;
		samplerDescriptorInfo.sampler = sampler.obj;

		std::array<vk::WriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eSampledImage;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &imageInfo;

		descriptorWrites[1].dstSet = descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = vk::DescriptorType::eSampler;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &samplerDescriptorInfo;

		device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto pixel = tester.readbackPixel(480, 420);
	EXPECT_GT(pixel[0], 200);
	EXPECT_LT(pixel[1], 80);
	EXPECT_LT(pixel[2], 80);
	EXPECT_GT(pixel[3], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, DynamicUniformBufferOffsetsSelectPerDrawColor)
{
	auto artifactPath = makeDrawArtifactPath("dynamic-uniform-buffer-offsets.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("dynamic-uniform-buffer-offsets");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	vk::Buffer uniformBuffer;
	vk::DeviceMemory uniformMemory;
	vk::DeviceSize uniformStride = 0;

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.50f,  0.95f, 0.5f } },
			{ { -0.05f, -0.85f, 0.5f } },
			{ {  0.05f, -0.85f, 0.5f } },
			{ {  0.50f,  0.95f, 0.5f } },
			{ {  0.95f, -0.85f, 0.5f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 450
			layout(location = 0) in vec3 inPos;

			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 450
			precision highp float;

			layout(set = 0, binding = 0) uniform ColorUBO
			{
				vec4 color;
			} ubo;

			layout(location = 0) out vec4 outColor;

			void main()
			{
				outColor = ubo.color;
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding uboBinding;
		uboBinding.binding = 0;
		uboBinding.descriptorCount = 1;
		uboBinding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
		uboBinding.pImmutableSamplers = nullptr;
		uboBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { uboBinding };
	});

	tester.onUpdateDescriptorSet([&](DrawTester &tester, vk::CommandPool &, vk::DescriptorSet &descriptorSet) {
		struct alignas(16) ColorBlock
		{
			float color[4];
		};

		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		const auto alignment = physicalDevice.getProperties().limits.minUniformBufferOffsetAlignment;
		auto alignTo = [](vk::DeviceSize value, vk::DeviceSize align) {
			if(align == 0)
			{
				return value;
			}
			return ((value + align - 1) / align) * align;
		};

		uniformStride = alignTo(sizeof(ColorBlock), alignment);
		const vk::DeviceSize bufferSize = uniformStride * 2;

		vk::BufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.size = bufferSize;
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
		uniformBuffer = device.createBuffer(bufferCreateInfo);

		const auto requirements = device.getBufferMemoryRequirements(uniformBuffer);
		vk::MemoryAllocateInfo allocateInfo;
		allocateInfo.allocationSize = requirements.size;
		allocateInfo.memoryTypeIndex = Util::getMemoryTypeIndex(physicalDevice, requirements.memoryTypeBits,
		                                                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		uniformMemory = device.allocateMemory(allocateInfo);
		device.bindBufferMemory(uniformBuffer, uniformMemory, 0);

		auto *mapped = static_cast<uint8_t *>(device.mapMemory(uniformMemory, 0, bufferSize));
		std::memset(mapped, 0, static_cast<size_t>(bufferSize));
		const ColorBlock leftColor = { { 1.0f, 0.0f, 0.0f, 1.0f } };
		const ColorBlock rightColor = { { 0.0f, 0.0f, 1.0f, 1.0f } };
		std::memcpy(mapped, &leftColor, sizeof(leftColor));
		std::memcpy(mapped + uniformStride, &rightColor, sizeof(rightColor));
		device.unmapMemory(uniformMemory);

		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(ColorBlock);

		vk::WriteDescriptorSet descriptorWrite;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
	});

	tester.onRecordDrawCommands([&](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		ASSERT_EQ(uniformStride % sizeof(uint32_t), 0u);
		ASSERT_LE(uniformStride, static_cast<vk::DeviceSize>(std::numeric_limits<uint32_t>::max()));
		const uint32_t rightOffset = static_cast<uint32_t>(uniformStride);

		tester.bindDescriptorSet(commandBuffer, { 0u });
		commandBuffer.draw(3, 1, 0, 0);

		tester.bindDescriptorSet(commandBuffer, { rightOffset });
		commandBuffer.draw(3, 1, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);

	auto leftPixel = tester.readbackPixel(320, 360);
	auto rightPixel = tester.readbackPixel(960, 360);

	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);

	EXPECT_LT(rightPixel[0], 80);
	EXPECT_LT(rightPixel[1], 80);
	EXPECT_GT(rightPixel[2], 200);

	EXPECT_TRUE(std::filesystem::exists(artifactPath));

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif

	auto &device = tester.getDevice();
	if(uniformBuffer)
	{
		device.destroyBuffer(uniformBuffer);
	}
	if(uniformMemory)
	{
		device.freeMemory(uniformMemory);
	}
}


TEST_F(DrawTest, IndexedTexturedTriangleNearest)
{
	auto artifactPath = makeDrawArtifactPath("indexed-textured-triangle-nearest.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-textured-triangle-nearest");
	auto sourceDumpPath = makeCudaLaunchStampPath("indexed-textured-triangle-nearest-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f,  0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
			{ {  0.95f,  0.95f, 0.5f }, { 1.0f, 1.0f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;
			void main() { outColor = texture(texSampler, inTexCoord); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &tester) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});

	tester.onUpdateDescriptorSet([](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm).obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint8_t *>(buffer.mapMemory());
		uint8_t pixels[] = {
			255u, 0u,   0u,   255u,
			0u,   255u, 0u,   255u,
			0u,   0u,   255u, 255u,
			255u, 255u, 0u,   255u,
		};
		std::memcpy(data, pixels, sizeof(pixels));
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eNearest;
		samplerInfo.minFilter = vk::Filter::eNearest;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
		auto sampler = tester.addSampler(samplerInfo);
		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();
		imageInfo.sampler = sampler.obj;
		std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 1;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &imageInfo;
		device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(3, 1, 0, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(480, 420);
	EXPECT_GT(pixel[3], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto sourceDump = readTextFile(sourceDumpPath);
	EXPECT_NE(sourceDump.find("textureData"), std::string::npos);
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, TexturedTriangleLinearRepeat)
{
	auto artifactPath = makeDrawArtifactPath("textured-triangle-linear-repeat.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("textured-triangle-linear-repeat");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { -0.25f, -0.25f } },
			{ { -0.20f,  0.95f, 0.5f }, { -0.25f,  1.25f } },
			{ {  0.95f, -0.85f, 0.5f }, {  1.25f, -0.25f } },
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;
			void main() { outColor = texture(texSampler, inTexCoord); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &tester) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});

	tester.onUpdateDescriptorSet([](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm).obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint8_t *>(buffer.mapMemory());
		uint8_t pixels[] = {
			255u,   0u,   0u, 255u,
			  0u, 255u,   0u, 255u,
			  0u,   0u, 255u, 255u,
			255u, 255u, 255u, 255u,
		};
		std::memcpy(data, pixels, sizeof(pixels));
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eLinear;
		samplerInfo.minFilter = vk::Filter::eLinear;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		auto sampler = tester.addSampler(samplerInfo);
		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();
		imageInfo.sampler = sampler.obj;
		std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {};
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 1;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &imageInfo;
		device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto centerPixel = tester.readbackPixel(640, 360);
	auto topPixel = tester.readbackPixel(640, 300);
	auto bottomPixel = tester.readbackPixel(640, 480);
	EXPECT_GT(centerPixel[0], 100);
	EXPECT_LT(centerPixel[1], 40);
	EXPECT_GT(centerPixel[2], 80);
	EXPECT_GT(topPixel[0], topPixel[2]);
	EXPECT_GT(topPixel[0], 180);
	EXPECT_GT(bottomPixel[2], bottomPixel[0]);
	EXPECT_GT(bottomPixel[2], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, TexturedTriangleDescriptorUpdateChangesFrame)
{
	auto artifactPath = makeDrawArtifactPath("textured-triangle-descriptor-update.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("textured-triangle-descriptor-update");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	int textureMode = 0;
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f,  0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;
			void main() { outColor = texture(texSampler, inTexCoord); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &tester) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});

	tester.onUpdateDescriptorSet([&](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled).obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint8_t *>(buffer.mapMemory());
		uint8_t pixels[16];
		for(int i = 0; i < 16; i += 4)
		{
			pixels[i + 0] = textureMode == 0 ? 255u : 0u;
			pixels[i + 1] = 0u;
			pixels[i + 2] = textureMode == 0 ? 0u : 255u;
			pixels[i + 3] = 255u;
		}
		std::memcpy(data, pixels, sizeof(pixels));
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eNearest;
		samplerInfo.minFilter = vk::Filter::eNearest;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		auto sampler = tester.addSampler(samplerInfo);
		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();
		imageInfo.sampler = sampler.obj;
		vk::WriteDescriptorSet descriptorWrite;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 1;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;
		device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
	});

	tester.initialize();
	tester.renderFrame();
	textureMode = 1;
	tester.updateDescriptorSetNow();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 320);
	EXPECT_LT(pixel[0], 80);
	EXPECT_LT(pixel[1], 80);
	EXPECT_GT(pixel[2], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, InstancedTexturedTriangles)
{
	auto artifactPath = makeDrawArtifactPath("instanced-textured-triangles.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("instanced-textured-triangles");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		struct Instance { float offset[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.20f, -0.45f, 0.5f }, { 0.0f, 0.0f } },
			{ {  0.00f,  0.05f, 0.5f }, { 0.0f, 1.0f } },
			{ {  0.20f, -0.45f, 0.5f }, { 1.0f, 0.0f } },
		};
		Instance instanceBufferData[] = {
			{ { -0.45f, 0.0f } },
			{ {  0.45f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(vertexAttributes));
		std::vector<vk::VertexInputAttributeDescription> instanceAttributes;
		instanceAttributes.push_back(vk::VertexInputAttributeDescription(2, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)));
		tester.addInstanceBuffer(instanceBufferData, sizeof(instanceBufferData), std::move(instanceAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 2) in vec2 inOffset;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xy + inOffset, inPos.z, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec2 outTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;
			void main() { outColor = texture(texSampler, outTexCoord); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &tester) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});

	tester.onUpdateDescriptorSet([](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled).obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint8_t *>(buffer.mapMemory());
		for(int i = 0; i < 16; i += 4)
		{
			data[i + 0] = 0u;
			data[i + 1] = 0u;
			data[i + 2] = 255u;
			data[i + 3] = 255u;
		}
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eNearest;
		samplerInfo.minFilter = vk::Filter::eNearest;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		auto sampler = tester.addSampler(samplerInfo);
		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();
		imageInfo.sampler = sampler.obj;
		vk::WriteDescriptorSet descriptorWrite;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 1;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;
		device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 2, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(320, 240);
	auto rightPixel = tester.readbackPixel(960, 240);
	EXPECT_LT(leftPixel[0], 80);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_GT(leftPixel[2], 200);
	EXPECT_LT(rightPixel[0], 80);
	EXPECT_LT(rightPixel[1], 80);
	EXPECT_GT(rightPixel[2], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, VertexInputDynamicStateInstancedTexturedTriangles)
{
	auto artifactPath = makeDrawArtifactPath("vertex-input-dynamic-state-instanced-textured-triangles.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-input-dynamic-state-instanced-textured-triangles");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableVertexInputDynamicState();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; float texCoord[2]; };
		struct Instance { float offset[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.20f, -0.45f, 0.5f }, { 0.0f, 0.0f } },
			{ {  0.00f,  0.05f, 0.5f }, { 0.0f, 1.0f } },
			{ {  0.20f, -0.45f, 0.5f }, { 1.0f, 0.0f } },
		};
		Instance instanceBufferData[] = {
			{ { -0.45f, 0.0f } },
			{ {  0.45f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(vertexAttributes));
		std::vector<vk::VertexInputAttributeDescription> instanceAttributes;
		instanceAttributes.push_back(vk::VertexInputAttributeDescription(2, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)));
		tester.addInstanceBuffer(instanceBufferData, sizeof(instanceBufferData), std::move(instanceAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 2) in vec2 inOffset;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos.xy + inOffset, inPos.z, 1.0);
				outTexCoord = inTexCoord;
			})";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) in vec2 outTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;
			void main() { outColor = texture(texSampler, outTexCoord); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &tester) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});

	tester.onUpdateDescriptorSet([](DrawTester &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet) {
		auto &device = tester.getDevice();
		auto &physicalDevice = tester.getPhysicalDevice();
		auto &queue = tester.getQueue();
		auto &texture = tester.addImage(device, physicalDevice, 2, 2, vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled).obj;
		Buffer buffer(physicalDevice, device, 2 * 2 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint8_t *>(buffer.mapMemory());
		for(int i = 0; i < 16; i += 4)
		{
			data[i + 0] = 0u;
			data[i + 1] = 0u;
			data[i + 2] = 255u;
			data[i + 3] = 255u;
		}
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 2, 2);
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::SamplerCreateInfo samplerInfo;
		samplerInfo.magFilter = vk::Filter::eNearest;
		samplerInfo.minFilter = vk::Filter::eNearest;
		samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		auto sampler = tester.addSampler(samplerInfo);
		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.getImageView();
		imageInfo.sampler = sampler.obj;
		vk::WriteDescriptorSet descriptorWrite;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 1;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;
		device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 2, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(320, 240);
	auto rightPixel = tester.readbackPixel(960, 240);
	EXPECT_LT(leftPixel[0], 80);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_GT(leftPixel[2], 200);
	EXPECT_LT(rightPixel[0], 80);
	EXPECT_LT(rightPixel[1], 80);
	EXPECT_GT(rightPixel[2], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}



TEST_F(DrawTest, ClearColorBackground)
{
	auto artifactPath = makeDrawArtifactPath("clear-color-background.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("clear-color-background");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.125f, 0.25f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.75f, -0.75f, 0.5f } },
			{ { -0.25f,  0.25f, 0.5f } },
			{ {  0.25f, -0.75f, 0.5f } },
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
	auto background = tester.readbackPixel(1100, 650);
	auto triangle = tester.readbackPixel(320, 240);
	EXPECT_NEAR(background[0], 32, 4);
	EXPECT_NEAR(background[1], 64, 4);
	EXPECT_NEAR(background[2], 128, 4);
	EXPECT_GT(triangle[0], 200);
	EXPECT_LT(triangle[1], 80);
	EXPECT_LT(triangle[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, VertexShaderUsesPushConstantOffset)
{
	auto artifactPath = makeDrawArtifactPath("push-constant-vertex-offset.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("push-constant-vertex-offset");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	struct PushConstants { float offset[2]; } push = { { 0.55f, 0.0f } };

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enablePushConstantRange(vk::ShaderStageFlagBits::eVertex, sizeof(PushConstants));
	tester.setPushConstantData(vk::ShaderStageFlagBits::eVertex, &push, sizeof(push));
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.55f,  0.25f, 0.5f } },
			{ { -0.15f, -0.85f, 0.5f } },
		};
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(push_constant) uniform PushConstants { vec2 offset; } pc;
			void main() { gl_Position = vec4(inPos.xy + pc.offset, inPos.z, 1.0); })";
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
	auto movedPixel = tester.readbackPixel(700, 240);
	auto background = tester.readbackPixel(220, 360);
	EXPECT_GT(movedPixel[0], 200);
	EXPECT_LT(movedPixel[1], 80);
	EXPECT_LT(movedPixel[2], 80);
	EXPECT_NEAR(background[0], 128, 4);
	EXPECT_NEAR(background[1], 128, 4);
	EXPECT_NEAR(background[2], 128, 4);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}

TEST_F(DrawTest, FragmentShaderUsesPushConstantTint)
{
	auto artifactPath = makeDrawArtifactPath("push-constant-fragment-tint.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("push-constant-fragment-tint");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	struct PushConstants { float tint[4]; } push = { { 0.2f, 0.8f, 0.4f, 1.0f } };

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enablePushConstantRange(vk::ShaderStageFlagBits::eFragment, sizeof(PushConstants));
	tester.setPushConstantData(vk::ShaderStageFlagBits::eFragment, &push, sizeof(push));
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.20f,  0.95f, 0.5f } },
			{ {  0.95f, -0.85f, 0.5f } },
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
			layout(push_constant) uniform PushConstants { vec4 tint; } pc;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = pc.tint; })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 360);
	EXPECT_NEAR(pixel[0], 51, 4);
	EXPECT_NEAR(pixel[1], 204, 4);
	EXPECT_NEAR(pixel[2], 102, 4);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, VertexShaderUsesInstanceIndexOffset)
{
	auto artifactPath = makeDrawArtifactPath("instance-index-offset.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("instance-index-offset");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.35f, -0.65f, 0.5f } },
			{ {  0.00f,  0.15f, 0.5f } },
			{ {  0.35f, -0.65f, 0.5f } },
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
				float xOffset = float(gl_InstanceIndex) * 0.8 - 0.4;
				gl_Position = vec4(inPos.x + xOffset, inPos.y, inPos.z, 1.0);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 2, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(380, 320);
	auto rightPixel = tester.readbackPixel(900, 320);
	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);
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


TEST_F(DrawTest, IndexedDrawUsesBaseVertexOffset)
{
	auto artifactPath = makeDrawArtifactPath("indexed-base-vertex-offset.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-base-vertex-offset");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.75f,  0.15f, 0.5f } },
			{ { -0.55f, -0.85f, 0.5f } },
			{ {  0.05f, -0.85f, 0.5f } },
			{ {  0.45f,  0.15f, 0.5f } },
			{ {  0.85f, -0.85f, 0.5f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u };
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(3, 1, 0, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto rightPixel = tester.readbackPixel(900, 240);
	auto leftBackground = tester.readbackPixel(260, 280);
	EXPECT_GT(rightPixel[0], 200);
	EXPECT_LT(rightPixel[1], 80);
	EXPECT_LT(rightPixel[2], 80);
	EXPECT_NEAR(leftBackground[0], 128, 4);
	EXPECT_NEAR(leftBackground[1], 128, 4);
	EXPECT_NEAR(leftBackground[2], 128, 4);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, VertexInputRateInstanceOffsets)
{
	auto artifactPath = makeDrawArtifactPath("instance-input-rate-offsets.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("instance-input-rate-offsets");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		struct Instance { float offset[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.20f, -0.45f, 0.5f } },
			{ {  0.00f,  0.05f, 0.5f } },
			{ {  0.20f, -0.45f, 0.5f } },
		};
		Instance instanceBufferData[] = {
			{ { -0.45f, 0.0f } },
			{ {  0.45f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(vertexAttributes));
		std::vector<vk::VertexInputAttributeDescription> instanceAttributes;
		instanceAttributes.push_back(vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)));
		tester.addInstanceBuffer(instanceBufferData, sizeof(instanceBufferData), std::move(instanceAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inOffset;
			void main() { gl_Position = vec4(inPos.xy + inOffset, inPos.z, 1.0); })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 2, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(320, 240);
	auto rightPixel = tester.readbackPixel(960, 240);
	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);
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

TEST_F(DrawTest, VertexInputDynamicStateInstanceRateOffsets)
{
	auto artifactPath = makeDrawArtifactPath("vertex-input-dynamic-state-instance-rate-offsets.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-input-dynamic-state-instance-rate-offsets");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableVertexInputDynamicState();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		struct Instance { float offset[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.20f, -0.45f, 0.5f } },
			{ {  0.00f,  0.05f, 0.5f } },
			{ {  0.20f, -0.45f, 0.5f } },
		};
		Instance instanceBufferData[] = {
			{ { -0.45f, 0.0f } },
			{ {  0.45f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(vertexAttributes));
		std::vector<vk::VertexInputAttributeDescription> instanceAttributes;
		instanceAttributes.push_back(vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)));
		tester.addInstanceBuffer(instanceBufferData, sizeof(instanceBufferData), std::move(instanceAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inOffset;
			void main() { gl_Position = vec4(inPos.xy + inOffset, inPos.z, 1.0); })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 2, 0, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(320, 240);
	auto rightPixel = tester.readbackPixel(960, 240);
	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);
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


TEST_F(DrawTest, ClearAttachmentsOverridesTriangleRegion)
{
	auto artifactPath = makeDrawArtifactPath("clear-attachments-overrides-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("clear-attachments-overrides-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.20f,  0.95f, 0.5f } },
			{ {  0.95f, -0.85f, 0.5f } },
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
		vk::ClearAttachment attachment;
		attachment.aspectMask = vk::ImageAspectFlagBits::eColor;
		attachment.colorAttachment = 0;
		attachment.clearValue.color = vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 1.0f, 1.0f });
		vk::ClearRect rect;
		rect.rect.offset = vk::Offset2D{ 520, 240 };
		rect.rect.extent = vk::Extent2D{ 240, 180 };
		rect.baseArrayLayer = 0;
		rect.layerCount = 1;
		commandBuffer.clearAttachments(1, &attachment, 1, &rect);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto clearedPixel = tester.readbackPixel(640, 300);
	auto unclearedTrianglePixel = tester.readbackPixel(420, 300);
	EXPECT_LT(clearedPixel[0], 80);
	EXPECT_LT(clearedPixel[1], 80);
	EXPECT_GT(clearedPixel[2], 200);
	EXPECT_GT(unclearedTrianglePixel[0], 200);
	EXPECT_LT(unclearedTrianglePixel[1], 80);
	EXPECT_LT(unclearedTrianglePixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, ColorLoadOpLoadPreservesPreviousFrame)
{
	auto artifactPath = makeDrawArtifactPath("color-load-op-load-preserves-previous-frame.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("color-load-op-load-preserves-previous-frame");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	struct Vertex
	{
		float position[3];
		float color[3];
	};

	Vertex fullscreenRedTriangle[] = {
		{ { -1.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ {  3.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { -1.0f,  3.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
	};
	Vertex smallGreenTriangle[] = {
		{ { -0.25f, -0.20f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.00f,  0.35f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.25f, -0.20f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
	};

	DrawTester tester;
	tester.enableColorLoad();
	tester.onCreateVertexBuffers([&](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));
		tester.addVertexBuffer(fullscreenRedTriangle, sizeof(fullscreenRedTriangle), std::move(inputAttributes));
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
	tester.renderFrame();
	tester.updateVertexBufferData(smallGreenTriangle, sizeof(smallGreenTriangle));
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto greenPixel = tester.readbackPixel(640, 320);
	auto preservedRedPixel = tester.readbackPixel(180, 140);
	EXPECT_LT(greenPixel[0], 80);
	EXPECT_GT(greenPixel[1], 200);
	EXPECT_LT(greenPixel[2], 80);
	EXPECT_GT(preservedRedPixel[0], 200);
	EXPECT_LT(preservedRedPixel[1], 80);
	EXPECT_LT(preservedRedPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, ClearAttachmentsDepthEnablesFarTriangleInRect)
{
	auto artifactPath = makeDrawArtifactPath("clear-attachments-depth-enables-far-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("clear-attachments-depth-enables-far-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableDepthTest(true, true, vk::CompareOp::eLessOrEqual);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		Vertex vertexBufferData[] = {
			{ { -1.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  3.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  3.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.00f,  0.35f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
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
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
		vk::ClearAttachment attachment;
		attachment.aspectMask = vk::ImageAspectFlagBits::eDepth;
		attachment.clearValue.depthStencil = vk::ClearDepthStencilValue(1.0f, 0u);
		vk::ClearRect rect;
		rect.rect.offset = vk::Offset2D{ 520, 220 };
		rect.rect.extent = vk::Extent2D{ 240, 220 };
		rect.baseArrayLayer = 0;
		rect.layerCount = 1;
		commandBuffer.clearAttachments(1, &attachment, 1, &rect);
		commandBuffer.draw(3, 1, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto greenPixel = tester.readbackPixel(640, 320);
	auto redPixel = tester.readbackPixel(180, 140);
	EXPECT_LT(greenPixel[0], 80);
	EXPECT_GT(greenPixel[1], 200);
	EXPECT_LT(greenPixel[2], 80);
	EXPECT_GT(redPixel[0], 200);
	EXPECT_LT(redPixel[1], 80);
	EXPECT_LT(redPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, IndexedInstancedDrawUsesBaseVertexAndFirstInstance)
{
	auto artifactPath = makeDrawArtifactPath("indexed-instanced-base-vertex-first-instance.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("indexed-instanced-base-vertex-first-instance");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		struct Instance { float offset[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.75f,  0.15f, 0.5f } },
			{ { -0.55f, -0.85f, 0.5f } },
			{ { -0.20f, -0.45f, 0.5f } },
			{ {  0.00f,  0.05f, 0.5f } },
			{ {  0.20f, -0.45f, 0.5f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u };
		Instance instanceBufferData[] = {
			{ { -0.65f, 0.0f } },
			{ { -0.20f, 0.0f } },
			{ {  0.45f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(vertexAttributes));
		std::vector<vk::VertexInputAttributeDescription> instanceAttributes;
		instanceAttributes.push_back(vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)));
		tester.addInstanceBuffer(instanceBufferData, sizeof(instanceBufferData), std::move(instanceAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inOffset;
			void main() { gl_Position = vec4(inPos.xy + inOffset, inPos.z, 1.0); })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(3, 2, 0, 3, 1);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(500, 260);
	auto rightPixel = tester.readbackPixel(980, 300);
	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);
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


TEST_F(DrawTest, TwoSubpassesOverlayGreenTriangleOnRedBackground)
{
	auto artifactPath = makeDrawArtifactPath("two-subpasses-overlay-green-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("two-subpasses-overlay-green-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableSecondSubpass();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		Vertex vertexBufferData[] = {
			{ { -1.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  3.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  3.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -0.25f, -0.20f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.00f,  0.35f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.25f, -0.20f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
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
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
	});
	tester.onRecordSecondSubpassCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto greenPixel = tester.readbackPixel(640, 320);
	auto redPixel = tester.readbackPixel(180, 140);
	EXPECT_LT(greenPixel[0], 80);
	EXPECT_GT(greenPixel[1], 200);
	EXPECT_LT(greenPixel[2], 80);
	EXPECT_GT(redPixel[0], 200);
	EXPECT_LT(redPixel[1], 80);
	EXPECT_LT(redPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, VertexInputDynamicStateIndexedInstancedDrawUsesBaseVertexAndFirstInstance)
{
	auto artifactPath = makeDrawArtifactPath("vertex-input-dynamic-state-indexed-instanced-base-vertex-first-instance.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-input-dynamic-state-indexed-instanced-base-vertex-first-instance");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableVertexInputDynamicState();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		struct Instance { float offset[2]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.75f,  0.15f, 0.5f } },
			{ { -0.55f, -0.85f, 0.5f } },
			{ { -0.20f, -0.45f, 0.5f } },
			{ {  0.00f,  0.05f, 0.5f } },
			{ {  0.20f, -0.45f, 0.5f } },
		};
		uint16_t indexBufferData[] = { 0u, 1u, 2u };
		Instance instanceBufferData[] = {
			{ { -0.65f, 0.0f } },
			{ { -0.20f, 0.0f } },
			{ {  0.45f, 0.0f } },
		};
		std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
		vertexAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(vertexAttributes));
		std::vector<vk::VertexInputAttributeDescription> instanceAttributes;
		instanceAttributes.push_back(vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, offsetof(Instance, offset)));
		tester.addInstanceBuffer(instanceBufferData, sizeof(instanceBufferData), std::move(instanceAttributes));
		tester.addIndexBuffer(indexBufferData, sizeof(indexBufferData), vk::IndexType::eUint16);
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inOffset;
			void main() { gl_Position = vec4(inPos.xy + inOffset, inPos.z, 1.0); })";
		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;
			layout(location = 0) out vec4 outColor;
			void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		tester.bindIndexBuffer(commandBuffer);
		commandBuffer.drawIndexed(3, 2, 0, 3, 1);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto leftPixel = tester.readbackPixel(500, 260);
	auto rightPixel = tester.readbackPixel(980, 300);
	EXPECT_GT(leftPixel[0], 200);
	EXPECT_LT(leftPixel[1], 80);
	EXPECT_LT(leftPixel[2], 80);
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


TEST_F(DrawTest, SwapchainMinImageCountTripleBufferedTriangle)
{
	auto artifactPath = makeDrawArtifactPath("swapchain-min-image-count-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("swapchain-min-image-count-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.setSwapchainMinImageCount(3);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.20f,  0.95f, 0.5f } },
			{ {  0.95f, -0.85f, 0.5f } },
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
	EXPECT_GE(tester.getSwapchainImageCount(), 3u);
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 240);
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


TEST_F(DrawTest, DepthLoadOpLoadPreservesPreviousDepth)
{
	auto artifactPath = makeDrawArtifactPath("depth-load-op-load-preserves-previous-depth.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("depth-load-op-load-preserves-previous-depth");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	struct Vertex
	{
		float position[3];
		float color[3];
	};

	Vertex fullscreenRedNear[] = {
		{ { -1.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ {  3.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { -1.0f,  3.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
	};
	Vertex smallGreenFar[] = {
		{ { -0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.00f,  0.35f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
	};

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableDepthTest(true, true, vk::CompareOp::eLessOrEqual);
	tester.enableDepthLoad();
	tester.onCreateVertexBuffers([&](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));
		tester.addVertexBuffer(fullscreenRedNear, sizeof(fullscreenRedNear), std::move(inputAttributes));
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
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	tester.renderFrame();
	tester.renderFrame();
	tester.updateVertexBufferData(smallGreenFar, sizeof(smallGreenFar));
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto centerPixel = tester.readbackPixel(640, 320);
	EXPECT_NEAR(centerPixel[0], 128, 6);
	EXPECT_NEAR(centerPixel[1], 128, 6);
	EXPECT_NEAR(centerPixel[2], 128, 6);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, TwoSubpassesDepthBlocksFarTriangle)
{
	auto artifactPath = makeDrawArtifactPath("two-subpasses-depth-blocks-far-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("two-subpasses-depth-blocks-far-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableDepthTest(true, true, vk::CompareOp::eLessOrEqual);
	tester.enableSecondSubpass();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		Vertex vertexBufferData[] = {
			{ { -1.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  3.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  3.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.00f,  0.35f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
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
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
	});
	tester.onRecordSecondSubpassCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto centerPixel = tester.readbackPixel(640, 320);
	auto cornerPixel = tester.readbackPixel(180, 140);
	EXPECT_GT(cornerPixel[0], 200);
	EXPECT_LT(cornerPixel[1], 80);
	EXPECT_LT(cornerPixel[2], 80);
	EXPECT_GT(centerPixel[0], 200);
	EXPECT_LT(centerPixel[1], 80);
	EXPECT_LT(centerPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, SwapchainTripleBufferedColorCycle)
{
	auto artifactPath = makeDrawArtifactPath("swapchain-triple-buffered-color-cycle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("swapchain-triple-buffered-color-cycle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	struct Vertex
	{
		float position[3];
		float color[3];
	};

	Vertex redTriangle[] = {
		{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { -0.20f,  0.95f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ {  0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
	};
	Vertex greenTriangle[] = {
		{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
		{ { -0.20f,  0.95f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
		{ {  0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
	};
	Vertex blueTriangle[] = {
		{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		{ { -0.20f,  0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
		{ {  0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
	};

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.setSwapchainMinImageCount(3);
	tester.onCreateVertexBuffers([&](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));
		tester.addVertexBuffer(redTriangle, sizeof(redTriangle), std::move(inputAttributes));
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
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.initialize();
	EXPECT_GE(tester.getSwapchainImageCount(), 3u);
	tester.renderFrame();
	tester.updateVertexBufferData(greenTriangle, sizeof(greenTriangle));
	tester.renderFrame();
	tester.updateVertexBufferData(blueTriangle, sizeof(blueTriangle));
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 240);
	EXPECT_LT(pixel[0], 80);
	EXPECT_LT(pixel[1], 80);
	EXPECT_GT(pixel[2], 200);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}



TEST_F(DrawTest, DrawUsesFirstInstanceOffset)
{
	auto artifactPath = makeDrawArtifactPath("first-instance-offset.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("first-instance-offset");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.35f, -0.65f, 0.5f } },
			{ {  0.00f,  0.15f, 0.5f } },
			{ {  0.35f, -0.65f, 0.5f } },
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
				float xOffset = float(gl_InstanceIndex) * 0.2 - 1.0;
				gl_Position = vec4(inPos.x + xOffset, inPos.y, inPos.z, 1.0);
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

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 5);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto pixel = tester.readbackPixel(640, 240);
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


TEST_F(DrawTest, MultisampleSolidColorTriangle)
{
	auto artifactPath = makeDrawArtifactPath("msaa-solid-color-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("msaa-solid-color-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester(Multisample::True);
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.20f,  0.95f, 0.5f } },
			{ {  0.95f, -0.85f, 0.5f } },
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
	auto pixel = tester.readbackPixel(640, 240);
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

TEST_F(DrawTest, MultisampleVertexColorTriangleInterpolation)
{
	auto artifactPath = makeDrawArtifactPath("msaa-vertex-color-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("msaa-vertex-color-triangle");
	auto sourceDumpPath = makeCudaLaunchStampPath("msaa-vertex-color-triangle-source");
	std::filesystem::remove(stampPath);
	std::filesystem::remove(sourceDumpPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH", sourceDumpPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester(Multisample::True);
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  0.95f, -0.85f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.0f,  0.95f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
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
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_SOURCE_DUMP_PATH");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}


TEST_F(DrawTest, MultisampleDepthBlocksFarTriangle)
{
	auto artifactPath = makeDrawArtifactPath("msaa-depth-blocks-far-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("msaa-depth-blocks-far-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester(Multisample::True);
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableDepthTest(true, true, vk::CompareOp::eLessOrEqual);
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		Vertex vertexBufferData[] = {
			{ { -1.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ {  3.0f, -1.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -1.0f,  3.0f, 0.5f }, { 1.0f, 0.0f, 0.0f } },
			{ { -0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.00f,  0.35f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
			{ {  0.25f, -0.20f, 0.75f }, { 0.0f, 1.0f, 0.0f } },
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
			void main() { outColor = vec4(vColor, 1.0); })";
		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onRecordDrawCommands([](DrawTester &tester, vk::CommandBuffer &commandBuffer) {
		commandBuffer.draw(3, 1, 0, 0);
		commandBuffer.draw(3, 1, 3, 0);
	});

	tester.initialize();
	tester.renderFrame();
	tester.saveFrame(artifactPath);
	auto centerPixel = tester.readbackPixel(640, 320);
	auto cornerPixel = tester.readbackPixel(180, 140);
	EXPECT_GT(cornerPixel[0], 200);
	EXPECT_LT(cornerPixel[1], 80);
	EXPECT_LT(cornerPixel[2], 80);
	EXPECT_GT(centerPixel[0], 200);
	EXPECT_LT(centerPixel[1], 80);
	EXPECT_LT(centerPixel[2], 80);
	EXPECT_TRUE(std::filesystem::exists(artifactPath));
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	EXPECT_GT(countStampedLaunches(stampPath), 0u);
	::unsetenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	::unsetenv("SWIFTSHADER_CUDA_DISABLE_WARMUP");
#endif
}



TEST_F(DrawTest, VertexInputDynamicStateSolidColorTriangle)
{
	auto artifactPath = makeDrawArtifactPath("vertex-input-dynamic-state-triangle.bmp");
	std::filesystem::remove(artifactPath);
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto stampPath = makeCudaLaunchStampPath("vertex-input-dynamic-state-triangle");
	std::filesystem::remove(stampPath);
	::setenv("SWIFTSHADER_CUDA_LAUNCH_STAMP", stampPath.c_str(), 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.enableVertexInputDynamicState();
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex { float position[3]; };
		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f } },
			{ { -0.20f,  0.95f, 0.5f } },
			{ {  0.95f, -0.85f, 0.5f } },
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
	auto pixel = tester.readbackPixel(640, 240);
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
