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

#ifndef DRAW_TESTER_HPP_
#define DRAW_TESTER_HPP_

#include "Buffer.hpp"
#include "Framebuffer.hpp"
#include "Image.hpp"
#include "Swapchain.hpp"
#include "Util.hpp"
#include "VulkanTester.hpp"
#include "Window.hpp"

#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

enum class Multisample
{
	False,
	True
};

class DrawTester : public VulkanTester
{
public:
	using ThisType = DrawTester;

	DrawTester(Multisample multisample = Multisample::False);
	~DrawTester();

	void initialize();
	void renderFrame();
	void renderFrameWithoutPresent();
	std::vector<uint8_t> readbackFrameRgba();
	std::array<uint8_t, 4> readbackPixel(uint32_t x, uint32_t y);
	void saveFrame(const std::filesystem::path &path);
	void updateVertexBufferData(const void *vertexBufferData, size_t vertexBufferDataSize);
	void updateDescriptorSetNow();
	void show();
	void setSwapchainMinImageCount(uint32_t minImageCount);
	size_t getSwapchainImageCount() const;
	void setPrimitiveTopology(vk::PrimitiveTopology topology);
	void setPrimitiveRestartEnable(bool enable);
	void setLineWidth(float width);
	void enableColorClear(const std::array<float, 4> &color);
	void enableColorLoad();
	void enableSecondSubpass();
	void enableVertexInputDynamicState();
	void enablePushConstantRange(vk::ShaderStageFlags stageFlags, uint32_t size);
	void setPushConstantData(vk::ShaderStageFlags stageFlags, const void *data, uint32_t size);
	void enableDynamicRendering();
	void enableDepthTest(bool depthTestEnable, bool depthWriteEnable, vk::CompareOp depthCompareOp);
	void enableDepthLoad();
	void pumpWindowEvents();
	void setWindowTitle(const std::string &title);
	void bindIndexBuffer(vk::CommandBuffer &commandBuffer);
	void bindDescriptorSet(vk::CommandBuffer &commandBuffer, std::initializer_list<uint32_t> dynamicOffsets = {});

	/////////////////////////
	// Hooks
	/////////////////////////

	// Called from prepareVertices.
	// Callback may call tester.addVertexBuffer() from this function.
	void onCreateVertexBuffers(std::function<void(ThisType &tester)> callback);

	// Called from createGraphicsPipeline.
	// Callback must return vector of DescriptorSetLayoutBindings for which a DescriptorSetLayout
	// will be created and stored in this->descriptorSetLayout.
	void onCreateDescriptorSetLayouts(std::function<std::vector<vk::DescriptorSetLayoutBinding>(ThisType &tester)> callback);

	// Called from createGraphicsPipeline.
	// Callback should call tester.createShaderModule() and return the result.
	void onCreateVertexShader(std::function<vk::ShaderModule(ThisType &tester)> callback);

	// Called from createGraphicsPipeline.
	// Callback should call tester.createShaderModule() and return the result.
	void onCreateFragmentShader(std::function<vk::ShaderModule(ThisType &tester)> callback);

	// Called from createCommandBuffers.
	// Callback may create resources (tester.addImage, tester.addSampler, etc.), and make sure to
	// call tester.device().updateDescriptorSets.
	void onUpdateDescriptorSet(std::function<void(ThisType &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet)> callback);

	// Called from createCommandBuffers after pipeline and vertex buffer binding.
	// Callback may record one or more draw commands. If unset, DrawTester records a single draw
	// covering all uploaded vertices.
	void onRecordDrawCommands(std::function<void(ThisType &tester, vk::CommandBuffer &commandBuffer)> callback);
	void onRecordSecondSubpassCommands(std::function<void(ThisType &tester, vk::CommandBuffer &commandBuffer)> callback);

	/////////////////////////
	// Resource Management
	/////////////////////////

	// Call from doCreateFragmentShader()
	vk::ShaderModule createShaderModule(const char *glslSource, EShLanguage glslLanguage);
	vk::ShaderModule createShaderModule(const std::vector<uint32_t> &spirv);

	// Call from doCreateVertexBuffers()
	template<typename VertexType>
	void addVertexBuffer(VertexType *vertexBufferData, size_t vertexBufferDataSize, std::vector<vk::VertexInputAttributeDescription> inputAttributes)
	{
		addVertexBuffer(vertexBufferData, vertexBufferDataSize, sizeof(VertexType), std::move(inputAttributes));
	}


	template<typename VertexType>
	void addInstanceBuffer(VertexType *vertexBufferData, size_t vertexBufferDataSize, std::vector<vk::VertexInputAttributeDescription> inputAttributes)
	{
		addInstanceBuffer(vertexBufferData, vertexBufferDataSize, sizeof(VertexType), std::move(inputAttributes));
	}

	template<typename IndexType>
	void addIndexBuffer(IndexType *indexBufferData, size_t indexBufferDataSize, vk::IndexType indexType)
	{
		addIndexBuffer(static_cast<void *>(indexBufferData), indexBufferDataSize, indexType);
	}

	template<typename T>
	struct Resource
	{
		size_t id;
		T &obj;
	};

	template<typename... Args>
	Resource<Image> addImage(Args &&...args)
	{
		images.emplace_back(std::make_unique<Image>(std::forward<Args>(args)...));
		return { images.size() - 1, *images.back() };
	}

	Image &getImageById(size_t id)
	{
		return *images[id].get();
	}

	Resource<vk::Sampler> addSampler(const vk::SamplerCreateInfo &samplerCreateInfo)
	{
		auto sampler = device.createSampler(samplerCreateInfo);
		samplers.push_back(sampler);
		return { samplers.size() - 1, samplers.back() };
	}

	vk::Sampler &getSamplerById(size_t id)
	{
		return samplers[id];
	}

private:
	void submitFrame(bool present);
	void createSynchronizationPrimitives();
	void createCommandBuffers(vk::RenderPass renderPass);
	void prepareVertices();
	void createFramebuffers(vk::RenderPass renderPass);
	vk::RenderPass createRenderPass(vk::Format colorFormat);
	vk::Pipeline createGraphicsPipeline(vk::RenderPass renderPass, uint32_t subpassIndex = 0);
	void addVertexBuffer(void *vertexBufferData, size_t vertexBufferDataSize, size_t vertexSize, std::vector<vk::VertexInputAttributeDescription> inputAttributes);
	void addInstanceBuffer(void *vertexBufferData, size_t vertexBufferDataSize, size_t vertexSize, std::vector<vk::VertexInputAttributeDescription> inputAttributes);
	void addIndexBuffer(void *indexBufferData, size_t indexBufferDataSize, vk::IndexType indexType);

	struct Hook
	{
		std::function<void(ThisType &tester)> createVertexBuffers = [](auto &) {};
		std::function<std::vector<vk::DescriptorSetLayoutBinding>(ThisType &tester)> createDescriptorSetLayout = [](auto &) { return std::vector<vk::DescriptorSetLayoutBinding>{}; };
		std::function<vk::ShaderModule(ThisType &tester)> createVertexShader = [](auto &) { return vk::ShaderModule{}; };
		std::function<vk::ShaderModule(ThisType &tester)> createFragmentShader = [](auto &) { return vk::ShaderModule{}; };
		std::function<void(ThisType &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet)> updateDescriptorSet = [](auto &, auto &, auto &) {};
		std::function<void(ThisType &tester, vk::CommandBuffer &commandBuffer)> recordDrawCommands;
		std::function<void(ThisType &tester, vk::CommandBuffer &commandBuffer)> recordSecondSubpassCommands;
	} hooks;

	const vk::Extent2D windowSize = { 1280, 720 };
	const bool multisample;
	bool depthTestEnabled = false;
	bool depthWriteEnabled = false;
	vk::CompareOp depthCompareOp = vk::CompareOp::eLessOrEqual;
	vk::AttachmentLoadOp depthLoadOp = vk::AttachmentLoadOp::eClear;
	vk::Format depthFormat = vk::Format::eD32Sfloat;
	vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
	bool primitiveRestartEnable = false;
	bool colorClearEnabled = false;
	bool secondSubpassEnabled = false;
	vk::AttachmentLoadOp colorLoadOp = vk::AttachmentLoadOp::eDontCare;
	std::array<float, 4> colorClearValue = { 0.5f, 0.5f, 0.5f, 1.0f };
	bool pushConstantEnabled = false;
	bool vertexInputDynamicStateEnabled = false;
	bool dynamicRenderingEnabled = false;
	vk::ShaderStageFlags pushConstantStages = {};
	uint32_t pushConstantSize = 0;
	std::array<uint8_t, 128> pushConstantData = {};
	float lineWidth = 1.0f;
	uint32_t swapchainMinImageCount = 2;

	std::unique_ptr<Window> window;
	std::unique_ptr<Swapchain> swapchain;

	vk::RenderPass renderPass;  // Owning handle
	std::vector<std::unique_ptr<Framebuffer>> framebuffers;
	uint32_t currentFrameBuffer = 0;

	struct VertexBuffer
	{
		vk::Buffer buffer;        // Owning handle
		vk::DeviceMemory memory;  // Owning handle
		size_t size = 0;

		vk::VertexInputBindingDescription inputBinding;
		std::vector<vk::VertexInputAttributeDescription> inputAttributes;
		vk::PipelineVertexInputStateCreateInfo inputState;

		uint32_t numVertices = 0;
	} vertices;

	VertexBuffer instances;

	struct IndexBuffer
	{
		vk::Buffer buffer;
		vk::DeviceMemory memory;
		size_t size = 0;
		vk::IndexType type = vk::IndexType::eUint16;
	} indices;

	vk::DescriptorSetLayout descriptorSetLayout;  // Owning handle
	std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;
	vk::PipelineLayout pipelineLayout;            // Owning handle
	vk::Pipeline pipeline;                        // Owning handle
	vk::Pipeline secondSubpassPipeline;           // Owning handle

	vk::Semaphore presentCompleteSemaphore;  // Owning handle
	vk::Semaphore renderCompleteSemaphore;   // Owning handle
	std::vector<vk::Fence> waitFences;       // Owning handles

	vk::CommandPool commandPool;        // Owning handle
	vk::DescriptorPool descriptorPool;  // Owning handle
	std::vector<vk::DescriptorSet> descriptorSets;

	// Resources
	std::vector<std::unique_ptr<Image>> images;
	std::vector<vk::Sampler> samplers;  // Owning handles

	std::vector<vk::CommandBuffer> commandBuffers;  // Owning handles
};

inline void DrawTester::onCreateVertexBuffers(std::function<void(ThisType &tester)> callback)
{
	hooks.createVertexBuffers = std::move(callback);
}

inline void DrawTester::onCreateDescriptorSetLayouts(std::function<std::vector<vk::DescriptorSetLayoutBinding>(ThisType &tester)> callback)
{
	hooks.createDescriptorSetLayout = std::move(callback);
}

inline void DrawTester::onCreateVertexShader(std::function<vk::ShaderModule(ThisType &tester)> callback)
{
	hooks.createVertexShader = std::move(callback);
}

inline void DrawTester::onCreateFragmentShader(std::function<vk::ShaderModule(ThisType &tester)> callback)
{
	hooks.createFragmentShader = std::move(callback);
}

inline void DrawTester::onUpdateDescriptorSet(std::function<void(ThisType &tester, vk::CommandPool &commandPool, vk::DescriptorSet &descriptorSet)> callback)
{
	hooks.updateDescriptorSet = std::move(callback);
}

inline void DrawTester::onRecordDrawCommands(std::function<void(ThisType &tester, vk::CommandBuffer &commandBuffer)> callback)
{
	hooks.recordDrawCommands = std::move(callback);
}

inline void DrawTester::onRecordSecondSubpassCommands(std::function<void(ThisType &tester, vk::CommandBuffer &commandBuffer)> callback)
{
	hooks.recordSecondSubpassCommands = std::move(callback);
}

#endif  // DRAW_TESTER_HPP_
