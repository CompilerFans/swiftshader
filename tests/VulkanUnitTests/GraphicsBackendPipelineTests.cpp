#include "Backend/FragmentBootstrap.hpp"
#include "DrawTester.hpp"
#include "PipelineIntrospection.hpp"

#include "gtest/gtest.h"

namespace {

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
			{ { 0.0f, -1.0f, 0.5f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;

			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				gl_PointSize = 7.0;
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

void configureTexturedTrianglePipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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

			void main()
			{
				outColor = texture(texSampler, inTexCoord);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});
}

void configureTexturedTrianglePipelineWithStorageImageWrite(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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
			layout(binding = 2, rgba8) uniform writeonly highp image2D storageImage;

			void main()
			{
				// Side effect that triangle bootstrap cannot emulate. If we accept the texture bootstrap
				// binding here, strict GPU bootstrap would silently skip the imageStore.
				imageStore(storageImage, ivec2(0, 0), vec4(1.0, 0.0, 0.0, 1.0));

				outColor = texture(texSampler, inTexCoord);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding storageImageLayoutBinding;
		storageImageLayoutBinding.binding = 2;
		storageImageLayoutBinding.descriptorCount = 1;
		storageImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		storageImageLayoutBinding.pImmutableSamplers = nullptr;
		storageImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		return { samplerLayoutBinding, storageImageLayoutBinding };
	});
}

void configureTexturedTrianglePipelineWithUniformGuard(DrawTester &tester, vk::DescriptorType uniformDescriptorType)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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

			layout(std140, binding = 0) uniform GuardBlock
			{
				float guardValue;
			};

			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;

			void main()
			{
				if(guardValue < -1000.0)
				{
					discard;
				}

				outColor = texture(texSampler, inTexCoord);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([uniformDescriptorType](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding uniformLayoutBinding;
		uniformLayoutBinding.binding = 0;
		uniformLayoutBinding.descriptorCount = 1;
		uniformLayoutBinding.descriptorType = uniformDescriptorType;
		uniformLayoutBinding.pImmutableSamplers = nullptr;
		uniformLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { uniformLayoutBinding, samplerLayoutBinding };
	});
}

void configureTexturedTrianglePipelineWithUniformGuard(DrawTester &tester)
{
	configureTexturedTrianglePipelineWithUniformGuard(tester, vk::DescriptorType::eUniformBuffer);
}

void configureTexturedTrianglePipelineWithDynamicUniformGuard(DrawTester &tester)
{
	configureTexturedTrianglePipelineWithUniformGuard(tester, vk::DescriptorType::eUniformBufferDynamic);
}

void configureTextureOnLocationOnePipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float color[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color));
		inputAttributes.emplace_back(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
		tester.addVertexBuffer(vertexBufferData, sizeof(vertexBufferData), std::move(inputAttributes));
	});

	tester.onCreateVertexShader([](DrawTester &tester) {
		const char *vertexShader = R"(#version 310 es
			layout(location = 0) in vec3 inPos;
			layout(location = 1) in vec3 inColor;
			layout(location = 2) in vec2 inTexCoord;
			layout(location = 0) out vec3 outColor;
			layout(location = 1) out vec2 outTexCoord;

			void main()
			{
				gl_Position = vec4(inPos.xyz, 1.0);
				outColor = inColor;
				outTexCoord = inTexCoord;
			})";

		return tester.createShaderModule(vertexShader, EShLanguage::EShLangVertex);
	});

	tester.onCreateFragmentShader([](DrawTester &tester) {
		const char *fragmentShader = R"(#version 310 es
			precision highp float;

			layout(location = 0) in vec3 inColor;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(binding = 1) uniform sampler2D texSampler;

			void main()
			{
				outColor = texture(texSampler, inTexCoord) * vec4(inColor, 1.0);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});
}

void configureTextureDescriptorArrayPipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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
			layout(binding = 1) uniform sampler2D texSampler[2];

			void main()
			{
				outColor = texture(texSampler[0], inTexCoord);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 2;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});
}

void configureTextureDescriptorArrayIndexOnePipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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
			layout(binding = 1) uniform sampler2D texSampler[2];

			void main()
			{
				outColor = texture(texSampler[1], inTexCoord);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 2;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});
}

void configureTextureScaledColorPipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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

			void main()
			{
				outColor = texture(texSampler, inTexCoord) * 0.5;
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});
}

void configureTextureSamplePassthroughPipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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

			void main()
			{
				vec4 sampledColor = texture(texSampler, inTexCoord);
				outColor = sampledColor;
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerLayoutBinding;
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;
		return { samplerLayoutBinding };
	});
}

void configureSeparateImageSamplerPipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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
}

void configureSeparateImageSamplerDescriptorArrayIndexOnePipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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
			layout(set = 0, binding = 0) uniform texture2D tex[2];
			layout(set = 0, binding = 1) uniform sampler texSampler[2];

			void main()
			{
				outColor = texture(sampler2D(tex[1], texSampler[1]), inTexCoord);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding imageBinding;
		imageBinding.binding = 0;
		imageBinding.descriptorCount = 2;
		imageBinding.descriptorType = vk::DescriptorType::eSampledImage;
		imageBinding.pImmutableSamplers = nullptr;
		imageBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding samplerBinding;
		samplerBinding.binding = 1;
		samplerBinding.descriptorCount = 2;
		samplerBinding.descriptorType = vk::DescriptorType::eSampler;
		samplerBinding.pImmutableSamplers = nullptr;
		samplerBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		return { imageBinding, samplerBinding };
	});
}

void configureSeparateImageSamplerPipelineWithUniformGuard(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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

			layout(std140, set = 0, binding = 2) uniform GuardBlock
			{
				float guardValue;
			};

			layout(location = 0) in vec2 inTexCoord;
			layout(location = 0) out vec4 outColor;
			layout(set = 0, binding = 0) uniform texture2D tex;
			layout(set = 0, binding = 1) uniform sampler texSampler;

			void main()
			{
				if(guardValue < -1000.0)
				{
					discard;
				}

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

		vk::DescriptorSetLayoutBinding uniformLayoutBinding;
		uniformLayoutBinding.binding = 2;
		uniformLayoutBinding.descriptorCount = 1;
		uniformLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uniformLayoutBinding.pImmutableSamplers = nullptr;
		uniformLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

		return { imageBinding, samplerBinding, uniformLayoutBinding };
	});
}

void configureMultiCombinedImageSamplerPipeline(DrawTester &tester)
{
	tester.onCreateVertexBuffers([](DrawTester &tester) {
		struct Vertex
		{
			float position[3];
			float texCoord[2];
		};

		Vertex vertexBufferData[] = {
			{ { -0.95f, -0.85f, 0.5f }, { 0.0f, 0.0f } },
			{ { -0.20f, 0.95f, 0.5f }, { 0.0f, 1.0f } },
			{ { 0.95f, -0.85f, 0.5f }, { 1.0f, 0.0f } },
		};

		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		inputAttributes.emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
		inputAttributes.emplace_back(1, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord));
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
			layout(binding = 1) uniform sampler2D texSamplerA;
			layout(binding = 2) uniform sampler2D texSamplerB;

			void main()
			{
				outColor = mix(texture(texSamplerA, inTexCoord), texture(texSamplerB, inTexCoord), 0.5);
			})";

		return tester.createShaderModule(fragmentShader, EShLanguage::EShLangFragment);
	});

	tester.onCreateDescriptorSetLayouts([](DrawTester &) -> std::vector<vk::DescriptorSetLayoutBinding> {
		vk::DescriptorSetLayoutBinding samplerBindingA;
		samplerBindingA.binding = 1;
		samplerBindingA.descriptorCount = 1;
		samplerBindingA.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerBindingA.pImmutableSamplers = nullptr;
		samplerBindingA.stageFlags = vk::ShaderStageFlagBits::eFragment;

		vk::DescriptorSetLayoutBinding samplerBindingB;
		samplerBindingB.binding = 2;
		samplerBindingB.descriptorCount = 1;
		samplerBindingB.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		samplerBindingB.pImmutableSamplers = nullptr;
		samplerBindingB.stageFlags = vk::ShaderStageFlagBits::eFragment;

		return { samplerBindingA, samplerBindingB };
	});
}

}  // namespace

TEST(GraphicsBackendPipeline, BuildsBackendExecutableForGraphicsPipeline)
{
	DrawTester tester;
	configureSolidColorTriangleDraw(tester);

	tester.initialize();

	EXPECT_TRUE(graphicsPipelineHasBackendExecutable(tester.getPipelineAddress()));
}

TEST(GraphicsBackendPipeline, ExtractsBootstrapPointSizeFromGraphicsPipeline)
{
	DrawTester tester;
	configureSolidColorTriangleDraw(tester);

	tester.initialize();

	EXPECT_FLOAT_EQ(graphicsPipelineBootstrapPointSize(tester.getPipelineAddress()), 7.0f);
}

TEST(GraphicsBackendPipeline, ExtractsConstantColorBootstrapFragmentTemplate)
{
	DrawTester tester;
	configureSolidColorTriangleDraw(tester);

	tester.initialize();

	const auto fragmentState = graphicsPipelineBootstrapFragmentState(tester.getPipelineAddress());
	ASSERT_TRUE(fragmentState.hasConfig);
	EXPECT_EQ(fragmentState.shaderKind, static_cast<uint32_t>(backend::FragmentBootstrapShaderKind::ConstantColor));
	EXPECT_FLOAT_EQ(fragmentState.colorR, 1.0f);
	EXPECT_FLOAT_EQ(fragmentState.colorG, 0.0f);
	EXPECT_FLOAT_EQ(fragmentState.colorB, 0.0f);
	EXPECT_FLOAT_EQ(fragmentState.colorA, 1.0f);
}

TEST(GraphicsBackendPipeline, ExtractsTextureBootstrapBindingFromGraphicsPipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	ASSERT_TRUE(textureState.hasBinding);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 1u);
	EXPECT_EQ(textureState.samplerDescriptorSet, 0u);
	EXPECT_EQ(textureState.samplerBinding, 1u);
}

TEST(GraphicsBackendPipeline, IgnoresNonSampledDescriptorsWhenClassifyingCombinedTexturePlan)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithUniformGuard(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	ASSERT_TRUE(textureState.hasBinding);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 1u);
	EXPECT_EQ(textureState.samplerDescriptorSet, 0u);
	EXPECT_EQ(textureState.samplerBinding, 1u);
}

TEST(GraphicsBackendPipeline, RejectsTextureBootstrapBindingWhenLocationZeroIsNotVec2TexCoord)
{
	DrawTester tester;
	configureTextureOnLocationOnePipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, ExtractsTextureBootstrapBindingForDescriptorArrayIndexZero)
{
	DrawTester tester;
	configureTextureDescriptorArrayPipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_TRUE(textureState.hasBinding);
	EXPECT_EQ(textureState.imageArrayElement, 0u);
	EXPECT_EQ(textureState.samplerArrayElement, 0u);
}

TEST(GraphicsBackendPipeline, ExtractsTextureBootstrapBindingForDescriptorArrayIndexOne)
{
	DrawTester tester;
	configureTextureDescriptorArrayIndexOnePipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_TRUE(textureState.hasBinding);
	EXPECT_EQ(textureState.imageArrayElement, 1u);
	EXPECT_EQ(textureState.samplerArrayElement, 1u);
}

TEST(GraphicsBackendPipeline, RejectsTextureBootstrapBindingWhenFragmentPostProcessesSampledColor)
{
	DrawTester tester;
	configureTextureScaledColorPipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, ExtractsTextureBootstrapBindingWhenFragmentUsesSamplePassthrough)
{
	DrawTester tester;
	configureTextureSamplePassthroughPipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	ASSERT_TRUE(textureState.hasBinding);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 1u);
	EXPECT_EQ(textureState.samplerDescriptorSet, 0u);
	EXPECT_EQ(textureState.samplerBinding, 1u);
}

TEST(GraphicsBackendPipeline, RejectsTextureBootstrapBindingForSeparateImageSampler)
{
	DrawTester tester;
	configureSeparateImageSamplerPipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::SeparateImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 0u);
	EXPECT_EQ(textureState.samplerDescriptorSet, 0u);
	EXPECT_EQ(textureState.samplerBinding, 1u);
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, IgnoresNonSampledDescriptorsWhenClassifyingSeparateTexturePlan)
{
	DrawTester tester;
	configureSeparateImageSamplerPipelineWithUniformGuard(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::SeparateImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 0u);
	EXPECT_EQ(textureState.samplerDescriptorSet, 0u);
	EXPECT_EQ(textureState.samplerBinding, 1u);
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, ExtractsSeparateTexturePlanArrayElementsForDescriptorArrayIndexOne)
{
	DrawTester tester;
	configureSeparateImageSamplerDescriptorArrayIndexOnePipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::SeparateImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 0u);
	EXPECT_EQ(textureState.imageArrayElement, 1u);
	EXPECT_EQ(textureState.samplerDescriptorSet, 0u);
	EXPECT_EQ(textureState.samplerBinding, 1u);
	EXPECT_EQ(textureState.samplerArrayElement, 1u);
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, ClassifiesMultipleCombinedImageSamplersAsOtherTexturePlan)
{
	DrawTester tester;
	configureMultiCombinedImageSamplerPipeline(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::Other));
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, RejectsTextureBootstrapBindingWhenFragmentHasStorageImageWrite)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithStorageImageWrite(tester);

	tester.initialize();

	const auto textureState = graphicsPipelineBootstrapTextureState(tester.getPipelineAddress());
	ASSERT_TRUE(textureState.hasPlan);
	EXPECT_EQ(textureState.resourceKind, static_cast<uint32_t>(GraphicsPipelineBootstrapTextureResourceKind::CombinedImageSampler));
	EXPECT_EQ(textureState.imageDescriptorSet, 0u);
	EXPECT_EQ(textureState.imageBinding, 1u);
	EXPECT_FALSE(textureState.hasBinding);
}

TEST(GraphicsBackendPipeline, ExtractsImageResourcePlanForCombinedTexturePipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipeline(tester);

	tester.initialize();

	const auto resourceState = graphicsPipelineImageResourceState(tester.getPipelineAddress());
	ASSERT_TRUE(resourceState.hasPlan);
	EXPECT_EQ(resourceState.sampledDescriptorCount, 1u);
	EXPECT_EQ(resourceState.storageDescriptorCount, 0u);
}

TEST(GraphicsBackendPipeline, ExtractsImageResourcePlanForSeparateImageSamplerPipeline)
{
	DrawTester tester;
	configureSeparateImageSamplerPipeline(tester);

	tester.initialize();

	const auto resourceState = graphicsPipelineImageResourceState(tester.getPipelineAddress());
	ASSERT_TRUE(resourceState.hasPlan);
	EXPECT_EQ(resourceState.sampledDescriptorCount, 2u);
	EXPECT_EQ(resourceState.storageDescriptorCount, 0u);
}

TEST(GraphicsBackendPipeline, ExtractsImageResourcePlanForStorageImageWritePipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithStorageImageWrite(tester);

	tester.initialize();

	const auto resourceState = graphicsPipelineImageResourceState(tester.getPipelineAddress());
	ASSERT_TRUE(resourceState.hasPlan);
	EXPECT_EQ(resourceState.sampledDescriptorCount, 1u);
	ASSERT_EQ(resourceState.storageDescriptorCount, 1u);
	EXPECT_EQ(resourceState.storageDescriptorSet, 0u);
	EXPECT_EQ(resourceState.storageBinding, 2u);
	EXPECT_EQ(resourceState.storageArrayElement, 0u);
	EXPECT_EQ(resourceState.storageDescriptorType, static_cast<uint32_t>(vk::DescriptorType::eStorageImage));

	const auto planState = graphicsPipelineResourcePlanState(tester.getPipelineAddress());
	ASSERT_TRUE(planState.hasPlan);
	EXPECT_NE(planState.fragmentFeatureMask & 2u, 0u);
}

TEST(GraphicsBackendPipeline, ExtractsResourcePlanForUniformGuardPipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithUniformGuard(tester);

	tester.initialize();

	const auto planState = graphicsPipelineResourcePlanState(tester.getPipelineAddress());
	ASSERT_TRUE(planState.hasPlan);
	EXPECT_EQ(planState.descriptorSetCount, 1u);
	EXPECT_EQ(planState.dynamicOffsetCount, 0u);
	EXPECT_EQ(planState.pushConstantSize, 128u);
	EXPECT_NE(planState.fragmentFeatureMask & 1u, 0u);
	EXPECT_GE(planState.descriptorRefCount, 2u);
	EXPECT_EQ(planState.bufferDescriptorCount, 1u);
	EXPECT_EQ(planState.firstBufferDescriptorSet, 0u);
	EXPECT_EQ(planState.firstBufferBinding, 0u);
	EXPECT_EQ(planState.firstBufferDescriptorType, static_cast<uint32_t>(vk::DescriptorType::eUniformBuffer));
	EXPECT_FALSE(planState.firstBufferIsDynamic);
}

TEST(GraphicsBackendPipeline, ExtractsDynamicOffsetsForDynamicUniformBufferPipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithDynamicUniformGuard(tester);

	tester.initialize();

	const auto planState = graphicsPipelineResourcePlanState(tester.getPipelineAddress());
	ASSERT_TRUE(planState.hasPlan);
	EXPECT_EQ(planState.descriptorSetCount, 1u);
	EXPECT_EQ(planState.dynamicOffsetCount, 1u);
	EXPECT_EQ(planState.pushConstantSize, 128u);
	EXPECT_NE(planState.fragmentFeatureMask & 1u, 0u);
	EXPECT_EQ(planState.bufferDescriptorCount, 1u);
	EXPECT_EQ(planState.firstBufferDescriptorSet, 0u);
	EXPECT_EQ(planState.firstBufferBinding, 0u);
	EXPECT_EQ(planState.firstBufferDescriptorType, static_cast<uint32_t>(vk::DescriptorType::eUniformBufferDynamic));
	EXPECT_TRUE(planState.firstBufferIsDynamic);
	EXPECT_EQ(planState.firstBufferDynamicOffsetIndex, 0u);
}

TEST(GraphicsBackendPipeline, MarksTriangleBootstrapUnsupportedReasonsForStorageImageWritePipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithStorageImageWrite(tester);

	tester.initialize();

	const auto planState = graphicsPipelineResourcePlanState(tester.getPipelineAddress());
	ASSERT_TRUE(planState.hasPlan);
	EXPECT_NE(planState.triangleBootstrapUnsupportedReasonMask &
	              static_cast<uint32_t>(GraphicsPipelineTriangleBootstrapUnsupportedReason::StorageImageReadWrite),
	          0u);
}

TEST(GraphicsBackendPipeline, MarksTriangleBootstrapUnsupportedReasonsForUniformGuardDiscardPipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipelineWithUniformGuard(tester);

	tester.initialize();

	const auto planState = graphicsPipelineResourcePlanState(tester.getPipelineAddress());
	ASSERT_TRUE(planState.hasPlan);
	EXPECT_NE(planState.triangleBootstrapUnsupportedReasonMask &
	              static_cast<uint32_t>(GraphicsPipelineTriangleBootstrapUnsupportedReason::BufferDescriptorsPresent),
	          0u);
	EXPECT_NE(planState.triangleBootstrapUnsupportedReasonMask &
	              static_cast<uint32_t>(GraphicsPipelineTriangleBootstrapUnsupportedReason::DiscardUnsupported),
	          0u);
}

TEST(GraphicsBackendPipeline, DoesNotMarkTriangleBootstrapUnsupportedReasonsForSimpleTexturePipeline)
{
	DrawTester tester;
	configureTexturedTrianglePipeline(tester);

	tester.initialize();

	const auto planState = graphicsPipelineResourcePlanState(tester.getPipelineAddress());
	ASSERT_TRUE(planState.hasPlan);
	EXPECT_EQ(planState.triangleBootstrapUnsupportedReasonMask, 0u);
}
