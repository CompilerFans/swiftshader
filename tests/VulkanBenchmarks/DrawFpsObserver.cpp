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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

enum class ObserverCase
{
	Solid,
	Many,
};

ObserverCase parseCase(const std::string &value)
{
	if(value == "many")
	{
		return ObserverCase::Many;
	}
	return ObserverCase::Solid;
}

int parseIntOption(const std::string &value, int fallback)
{
	char *end = nullptr;
	long parsed = std::strtol(value.c_str(), &end, 10);
	if(end == value.c_str() || *end != '\0')
	{
		return fallback;
	}
	return static_cast<int>(parsed);
}

void configureSolidColorTriangle(DrawTester &tester)
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
				outColor = vec4(1.0, 1.0, 1.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});
}

void configureManySolidTriangles(DrawTester &tester, int triangleCount)
{
	struct Vertex
	{
		float position[3];
	};

	auto vertexBufferData = std::make_shared<std::vector<Vertex>>();
	vertexBufferData->reserve(static_cast<size_t>(triangleCount) * 3);

	int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(triangleCount)))));
	int rows = std::max(1, (triangleCount + columns - 1) / columns);
	float cellWidth = 2.0f / static_cast<float>(columns);
	float cellHeight = 2.0f / static_cast<float>(rows);

	for(int triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
	{
		int column = triangleIndex % columns;
		int row = triangleIndex / columns;

		float left = -1.0f + column * cellWidth + cellWidth * 0.1f;
		float right = -1.0f + (column + 1) * cellWidth - cellWidth * 0.1f;
		float top = 1.0f - row * cellHeight - cellHeight * 0.1f;
		float bottom = 1.0f - (row + 1) * cellHeight + cellHeight * 0.1f;
		float centerX = 0.5f * (left + right);

		vertexBufferData->push_back({ { centerX, top, 0.5f } });
		vertexBufferData->push_back({ { left, bottom, 0.5f } });
		vertexBufferData->push_back({ { right, bottom, 0.5f } });
	}

	tester.onCreateVertexBuffers([vertexBufferData](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));

		tester.addVertexBuffer(vertexBufferData->data(), vertexBufferData->size() * sizeof(Vertex), std::move(inputAttributes));
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
				outColor = vec4(1.0, 1.0, 1.0, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});
}

}  // namespace

int main(int argc, char **argv)
{
	ObserverCase whichCase = ObserverCase::Solid;
	int seconds = 10;
	int triangleCount = 16384;

	for(int index = 1; index < argc; index++)
	{
		std::string argument = argv[index];
		if(argument.rfind("--case=", 0) == 0)
		{
			whichCase = parseCase(argument.substr(7));
		}
		else if(argument.rfind("--seconds=", 0) == 0)
		{
			seconds = std::max(1, parseIntOption(argument.substr(10), seconds));
		}
		else if(argument.rfind("--triangles=", 0) == 0)
		{
			triangleCount = std::max(1, parseIntOption(argument.substr(12), triangleCount));
		}
	}

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	::setenv("SWIFTSHADER_CUDA_DUMP_SOURCE", "0", 1);
	::setenv("SWIFTSHADER_CUDA_DISABLE_WARMUP", "1", 1);
#endif

	DrawTester tester;
	if(whichCase == ObserverCase::Many)
	{
		configureManySolidTriangles(tester, triangleCount);
	}
	else
	{
		configureSolidColorTriangle(tester);
	}

	tester.initialize();
	tester.show();

#if USE_HEADLESS_SURFACE
	std::cout << "headless surface active; no visible window on this platform" << std::endl;
#endif

	auto start = std::chrono::steady_clock::now();
	auto intervalStart = start;
	uint64_t totalFrames = 0;
	uint64_t intervalFrames = 0;

	while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < seconds)
	{
		tester.renderFrame();
		totalFrames++;
		intervalFrames++;

		auto now = std::chrono::steady_clock::now();
		auto intervalSeconds = std::chrono::duration<double>(now - intervalStart).count();
		if(intervalSeconds >= 1.0)
		{
			double totalSeconds = std::chrono::duration<double>(now - start).count();
			double currentFps = intervalFrames / intervalSeconds;
			double averageFps = totalFrames / totalSeconds;
			double frameMs = 1000.0 / std::max(currentFps, 0.0001);

			std::cout << std::fixed << std::setprecision(2)
			          << "window_fps current=" << currentFps
			          << " avg=" << averageFps
			          << " frame_ms=" << frameMs
			          << " case=" << (whichCase == ObserverCase::Many ? "ManySolidTriangles" : "SolidColorTriangle")
			          << " backend=cpu";
			if(whichCase == ObserverCase::Many)
			{
				std::cout << " triangles=" << triangleCount;
			}
			std::cout << std::endl;

			intervalStart = now;
			intervalFrames = 0;
		}
	}

	return 0;
}
