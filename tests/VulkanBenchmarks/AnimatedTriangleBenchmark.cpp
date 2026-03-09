#include "BenchmarkEnvironment.hpp"
#include "DrawTester.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

struct Vertex
{
	float position[3];
	float color[3];
};

struct TexturedVertex
{
	float position[3];
	float texCoord[2];
};

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

float phaseColor(float phase)
{
	return 0.5f + 0.5f * std::sin(phase);
}

std::array<Vertex, 3> buildAnimatedTriangle(float seconds)
{
	constexpr float basePositions[3][2] = {
		{ 0.0f, 0.72f },
		{ -0.68f, -0.48f },
		{ 0.68f, -0.48f },
	};

	float angle = seconds * 0.9f;
	float cosAngle = std::cos(angle);
	float sinAngle = std::sin(angle);

	std::array<Vertex, 3> vertices = {};
	for(size_t i = 0; i < vertices.size(); i++)
	{
		float x = basePositions[i][0];
		float y = basePositions[i][1];
		vertices[i].position[0] = x * cosAngle - y * sinAngle;
		vertices[i].position[1] = x * sinAngle + y * cosAngle;
		vertices[i].position[2] = 0.5f;

		float phase = seconds * 1.7f + static_cast<float>(i) * 2.0943951f;
		vertices[i].color[0] = phaseColor(phase);
		vertices[i].color[1] = phaseColor(phase + 2.0943951f);
		vertices[i].color[2] = phaseColor(phase + 4.1887902f);
	}

	return vertices;
}


std::array<TexturedVertex, 3> buildAnimatedTexturedTriangle(float seconds)
{
	constexpr float basePositions[3][2] = {
		{ 0.0f, 0.72f },
		{ -0.68f, -0.48f },
		{ 0.68f, -0.48f },
	};
	constexpr float baseTexCoords[3][2] = {
		{ 0.5f, 0.0f },
		{ 0.0f, 1.0f },
		{ 1.0f, 1.0f },
	};
	float angle = seconds * 0.9f;
	float cosAngle = std::cos(angle);
	float sinAngle = std::sin(angle);
	float texOffset = std::sin(seconds * 0.7f) * 0.25f;
	std::array<TexturedVertex, 3> vertices = {};
	for(size_t i = 0; i < vertices.size(); i++)
	{
		float x = basePositions[i][0];
		float y = basePositions[i][1];
		vertices[i].position[0] = x * cosAngle - y * sinAngle;
		vertices[i].position[1] = x * sinAngle + y * cosAngle;
		vertices[i].position[2] = 0.5f;
		vertices[i].texCoord[0] = baseTexCoords[i][0] + texOffset;
		vertices[i].texCoord[1] = baseTexCoords[i][1] - texOffset;
	}
	return vertices;
}

void configureAnimatedTexturedTriangle(DrawTester &tester, const std::array<TexturedVertex, 3> &initialVertices)
{
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([initialVertices](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(TexturedVertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(TexturedVertex, texCoord)));
		tester.addVertexBuffer(const_cast<TexturedVertex *>(initialVertices.data()), sizeof(initialVertices), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec2 outTexCoord;
			void main()
			{
				gl_Position = vec4(inPos, 1.0);
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
			void main()
			{
				outColor = texture(texSampler, outTexCoord);
			})";
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
		auto &texture = tester.addImage(device, physicalDevice, 16, 16, vk::Format::eR8G8B8A8Unorm).obj;
		Buffer buffer(physicalDevice, device, 16 * 16 * 4, vk::BufferUsageFlagBits::eTransferSrc);
		auto *data = static_cast<uint32_t *>(buffer.mapMemory());
		std::array<uint32_t, 4> rgba = { 0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFFFFFFFFu };
		for(int y = 0; y < 16; y++)
		{
			for(int x = 0; x < 16; x++)
			{
				data[y * 16 + x] = rgba[((x / 4) + (y / 4)) % rgba.size()];
			}
		}
		buffer.unmapMemory();
		Util::transitionImageLayout(device, commandPool, queue, texture.getImage(), vk::Format::eR8G8B8A8Unorm, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal);
		Util::copyBufferToImage(device, commandPool, queue, buffer.getBuffer(), texture.getImage(), 16, 16);
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
}

void configureAnimatedTriangle(DrawTester &tester, const std::array<Vertex, 3> &initialVertices)
{
	tester.enableColorClear({ 0.5f, 0.5f, 0.5f, 1.0f });
	tester.onCreateVertexBuffers([initialVertices](DrawTester &tester) {
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.push_back(vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)));
		inputAttributes.push_back(vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)));
		tester.addVertexBuffer(const_cast<Vertex *>(initialVertices.data()), sizeof(initialVertices), std::move(inputAttributes));
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
}

}  // namespace

int main(int argc, char **argv)
{
	int seconds = 10;
	std::string backendLabel = "cpu";
	std::string scene = "color";

	for(int index = 1; index < argc; index++)
	{
		std::string argument = argv[index];
		if(argument.rfind("--seconds=", 0) == 0)
		{
			seconds = std::max(1, parseIntOption(argument.substr(10), seconds));
		}
		else if(argument.rfind("--backend-label=", 0) == 0)
		{
			backendLabel = argument.substr(16);
		}
		else if(argument.rfind("--scene=", 0) == 0)
		{
			scene = argument.substr(8);
		}
	}

	benchmarkutil::configureRuntimeEnvironment();

	DrawTester tester;
	if(scene == "texture")
	{
		auto initialVertices = buildAnimatedTexturedTriangle(0.0f);
		configureAnimatedTexturedTriangle(tester, initialVertices);
	}
	else
	{
		auto initialVertices = buildAnimatedTriangle(0.0f);
		configureAnimatedTriangle(tester, initialVertices);
	}
	tester.initialize();
	tester.show();

	auto start = std::chrono::steady_clock::now();
	auto intervalStart = start;
	uint64_t totalFrames = 0;
	uint64_t intervalFrames = 0;

	while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < seconds)
	{
		double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		if(scene == "texture")
		{
			auto vertices = buildAnimatedTexturedTriangle(static_cast<float>(elapsedSeconds));
			tester.updateVertexBufferData(vertices.data(), sizeof(vertices));
		}
		else
		{
			auto vertices = buildAnimatedTriangle(static_cast<float>(elapsedSeconds));
			tester.updateVertexBufferData(vertices.data(), sizeof(vertices));
		}
		tester.renderFrame();
		tester.pumpWindowEvents();

		totalFrames++;
		intervalFrames++;

		auto now = std::chrono::steady_clock::now();
		auto intervalSeconds = std::chrono::duration<double>(now - intervalStart).count();
		if(intervalSeconds >= 1.0)
		{
			double totalSeconds = std::chrono::duration<double>(now - start).count();
			double currentFps = intervalFrames / intervalSeconds;
			double averageFps = totalFrames / totalSeconds;

			std::ostringstream title;
			title << std::fixed << std::setprecision(2)
			      << "Animated Triangle [" << backendLabel << "]"
			      << " current FPS=" << currentFps
			      << " avg FPS=" << averageFps;
			tester.setWindowTitle(title.str());

			std::cout << std::fixed << std::setprecision(2)
			          << "window_fps current=" << currentFps
			          << " avg=" << averageFps
			          << " case=" << (scene == "texture" ? "AnimatedTextureTriangle" : "AnimatedColorTriangle")
			          << " backend=" << backendLabel
			          << std::endl;

			intervalStart = now;
			intervalFrames = 0;
		}
	}

	return 0;
}
