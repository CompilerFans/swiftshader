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
#include <array>
#include <cstring>
#include <cstdint>
#include <fstream>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

DrawTester::DrawTester(Multisample multisample)
    : multisample(multisample == Multisample::True)
{
}

DrawTester::~DrawTester()
{
	if(!device)
	{
		return;
	}

	device.waitIdle();

	device.freeCommandBuffers(commandPool, commandBuffers);

	device.destroyDescriptorPool(descriptorPool);
	for(auto &sampler : samplers)
	{
		device.destroySampler(sampler, nullptr);
	}
	images.clear();
	device.destroyCommandPool(commandPool, nullptr);

	for(auto &fence : waitFences)
	{
		device.destroyFence(fence, nullptr);
	}

	device.destroySemaphore(renderCompleteSemaphore, nullptr);
	device.destroySemaphore(presentCompleteSemaphore, nullptr);

	if(secondSubpassPipeline)
	{
		device.destroyPipeline(secondSubpassPipeline);
	}
	device.destroyPipeline(pipeline);
	device.destroyPipelineLayout(pipelineLayout, nullptr);
	device.destroyDescriptorSetLayout(descriptorSetLayout);

	if(indices.memory)
	{
		device.freeMemory(indices.memory, nullptr);
	}
	if(indices.buffer)
	{
		device.destroyBuffer(indices.buffer, nullptr);
	}

	device.freeMemory(vertices.memory, nullptr);
	device.destroyBuffer(vertices.buffer, nullptr);

	for(auto &framebuffer : framebuffers)
	{
		framebuffer.reset();
	}

	device.destroyRenderPass(renderPass, nullptr);

	swapchain.reset();
	window.reset();
}

void DrawTester::initialize()
{
	VulkanTester::initialize();

	window.reset(new Window(instance, windowSize));
	swapchain.reset(new Swapchain(physicalDevice, device, *window, swapchainMinImageCount));

	if(!dynamicRenderingEnabled)
	{
		renderPass = createRenderPass(swapchain->colorFormat);
		createFramebuffers(renderPass);
	}

	prepareVertices();

	pipeline = createGraphicsPipeline(renderPass, 0);
	if(secondSubpassEnabled)
	{
		assert(!dynamicRenderingEnabled);
		secondSubpassPipeline = createGraphicsPipeline(renderPass, 1);
	}

	createSynchronizationPrimitives();

	createCommandBuffers(renderPass);
}

void DrawTester::submitFrame(bool present)
{
	swapchain->acquireNextImage(presentCompleteSemaphore, currentFrameBuffer);

	device.waitForFences(1, &waitFences[currentFrameBuffer], VK_TRUE, UINT64_MAX);
	device.resetFences(1, &waitFences[currentFrameBuffer]);

	vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk::SubmitInfo submitInfo;
	submitInfo.pWaitDstStageMask = &waitStageMask;
	submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderCompleteSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrameBuffer];
	submitInfo.commandBufferCount = 1;

	queue.submit(1, &submitInfo, waitFences[currentFrameBuffer]);

	if(present)
	{
		swapchain->queuePresent(queue, currentFrameBuffer, renderCompleteSemaphore);
	}
	else
	{
		device.waitForFences(1, &waitFences[currentFrameBuffer], VK_TRUE, UINT64_MAX);
	}
}

void DrawTester::renderFrame()
{
	submitFrame(true);
}

void DrawTester::renderFrameWithoutPresent()
{
	submitFrame(false);
}

std::vector<uint8_t> DrawTester::readbackFrameRgba()
{
	queue.waitIdle();

	vk::Image image = swapchain->getImage(currentFrameBuffer);
	Buffer readback(physicalDevice, device, windowSize.width * windowSize.height * 4, vk::BufferUsageFlagBits::eTransferDst);

	Util::transitionImageLayout(device, commandPool, queue, image, swapchain->colorFormat, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal);
	Util::copyImageToBuffer(device, commandPool, queue, image, readback.getBuffer(), windowSize.width, windowSize.height);
	Util::transitionImageLayout(device, commandPool, queue, image, swapchain->colorFormat, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::ePresentSrcKHR);

	auto *pixels = reinterpret_cast<const uint8_t *>(readback.mapMemory());
	std::vector<uint8_t> rgba(static_cast<size_t>(windowSize.width) * windowSize.height * 4);
	for(size_t offset = 0; offset < rgba.size(); offset += 4)
	{
		rgba[offset + 0] = pixels[offset + 2];
		rgba[offset + 1] = pixels[offset + 1];
		rgba[offset + 2] = pixels[offset + 0];
		rgba[offset + 3] = pixels[offset + 3];
	}
	readback.unmapMemory();
	return rgba;
}

std::array<uint8_t, 4> DrawTester::readbackPixel(uint32_t x, uint32_t y)
{
	auto rgba = readbackFrameRgba();
	size_t offset = (static_cast<size_t>(y) * windowSize.width + x) * 4;
	return { rgba[offset + 0], rgba[offset + 1], rgba[offset + 2], rgba[offset + 3] };
}

void DrawTester::saveFrame(const std::filesystem::path &path)
{
	queue.waitIdle();

	if(path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path());
	}

	vk::Image image = swapchain->getImage(currentFrameBuffer);
	size_t imageSize = static_cast<size_t>(windowSize.width) * windowSize.height * 4;
	Buffer readback(physicalDevice, device, imageSize, vk::BufferUsageFlagBits::eTransferDst);

	Util::transitionImageLayout(device, commandPool, queue, image, swapchain->colorFormat, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal);
	Util::copyImageToBuffer(device, commandPool, queue, image, readback.getBuffer(), windowSize.width, windowSize.height);
	Util::transitionImageLayout(device, commandPool, queue, image, swapchain->colorFormat, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::ePresentSrcKHR);

	auto *pixels = reinterpret_cast<const uint8_t *>(readback.mapMemory());

	std::ofstream stream(path, std::ios::binary);
	assert(stream.good());

	auto writeLE16 = [&stream](uint16_t value) {
		stream.put(static_cast<char>(value & 0xFF));
		stream.put(static_cast<char>((value >> 8) & 0xFF));
	};

	auto writeLE32 = [&stream](uint32_t value) {
		stream.put(static_cast<char>(value & 0xFF));
		stream.put(static_cast<char>((value >> 8) & 0xFF));
		stream.put(static_cast<char>((value >> 16) & 0xFF));
		stream.put(static_cast<char>((value >> 24) & 0xFF));
	};

	auto writeLE32Signed = [&writeLE32](int32_t value) {
		writeLE32(static_cast<uint32_t>(value));
	};

	constexpr uint32_t fileHeaderSize = 14;
	constexpr uint32_t infoHeaderSize = 40;
	uint32_t pixelDataOffset = fileHeaderSize + infoHeaderSize;
	uint32_t fileSize = pixelDataOffset + static_cast<uint32_t>(imageSize);

	stream.put('B');
	stream.put('M');
	writeLE32(fileSize);
	writeLE16(0);
	writeLE16(0);
	writeLE32(pixelDataOffset);

	writeLE32(infoHeaderSize);
	writeLE32(windowSize.width);
	writeLE32Signed(-static_cast<int32_t>(windowSize.height));
	writeLE16(1);
	writeLE16(32);
	writeLE32(0);
	writeLE32(static_cast<uint32_t>(imageSize));
	writeLE32(0);
	writeLE32(0);
	writeLE32(0);
	writeLE32(0);

	stream.write(reinterpret_cast<const char *>(pixels), imageSize);
	stream.close();

	readback.unmapMemory();
}

void DrawTester::updateVertexBufferData(const void *vertexBufferData, size_t vertexBufferDataSize)
{
	assert(vertices.buffer);
	assert(vertices.memory);
	assert(vertexBufferData != nullptr);
	assert(vertexBufferDataSize <= vertices.size);

	void *data = device.mapMemory(vertices.memory, 0, vertices.size);
	std::memcpy(data, vertexBufferData, vertexBufferDataSize);
	device.unmapMemory(vertices.memory);
}

void DrawTester::updateDescriptorSetNow()
{
	if(descriptorSets.empty())
	{
		return;
	}
	device.waitIdle();
	hooks.updateDescriptorSet(*this, commandPool, descriptorSets[0]);
}

void DrawTester::show()
{
	window->show();
}

void DrawTester::setSwapchainMinImageCount(uint32_t minImageCount)
{
	swapchainMinImageCount = std::max(2u, minImageCount);
}

size_t DrawTester::getSwapchainImageCount() const
{
	return swapchain ? swapchain->imageCount() : 0;
}

void DrawTester::setPrimitiveTopology(vk::PrimitiveTopology topology)
{
	primitiveTopology = topology;
}

void DrawTester::setPrimitiveRestartEnable(bool enable)
{
	primitiveRestartEnable = enable;
}

void DrawTester::setLineWidth(float width)
{
	lineWidth = width;
}

void DrawTester::enableColorClear(const std::array<float, 4> &color)
{
	colorClearEnabled = true;
	colorLoadOp = vk::AttachmentLoadOp::eClear;
	colorClearValue = color;
}

void DrawTester::enableColorLoad()
{
	colorClearEnabled = false;
	colorLoadOp = vk::AttachmentLoadOp::eLoad;
}

void DrawTester::enableSecondSubpass()
{
	secondSubpassEnabled = true;
}

void DrawTester::enableVertexInputDynamicState()
{
	vertexInputDynamicStateEnabled = true;
}

void DrawTester::enablePushConstantRange(vk::ShaderStageFlags stageFlags, uint32_t size)
{
	assert(size <= vk::MAX_PUSH_CONSTANT_SIZE);
	pushConstantEnabled = true;
	pushConstantStages = stageFlags;
	pushConstantSize = size;
}

void DrawTester::setPushConstantData(vk::ShaderStageFlags stageFlags, const void *data, uint32_t size)
{
	assert(data != nullptr);
	assert(size <= vk::MAX_PUSH_CONSTANT_SIZE);
	pushConstantEnabled = true;
	pushConstantStages = stageFlags;
	pushConstantSize = size;
	std::memcpy(pushConstantData.data(), data, size);
}

void DrawTester::enableDynamicRendering()
{
	dynamicRenderingEnabled = true;
}

void DrawTester::enableDepthTest(bool enableTest, bool enableWrite, vk::CompareOp compareOp)
{
	depthTestEnabled = enableTest;
	depthWriteEnabled = enableWrite;
	depthCompareOp = compareOp;
}

void DrawTester::enableDepthLoad()
{
	depthLoadOp = vk::AttachmentLoadOp::eLoad;
}

void DrawTester::pumpWindowEvents()
{
	window->pumpEvents();
}

void DrawTester::setWindowTitle(const std::string &title)
{
	window->setTitle(title);
}

void DrawTester::bindIndexBuffer(vk::CommandBuffer &commandBuffer)
{
	assert(indices.buffer);
	commandBuffer.bindIndexBuffer(indices.buffer, 0, indices.type);
}

void DrawTester::bindDescriptorSet(vk::CommandBuffer &commandBuffer, std::initializer_list<uint32_t> dynamicOffsets)
{
	if(descriptorSets.empty())
	{
		return;
	}

	uint32_t expectedDynamicOffsetCount = 0;
	for(const auto &binding : descriptorSetLayoutBindings)
	{
		if(binding.descriptorType == vk::DescriptorType::eUniformBufferDynamic ||
		   binding.descriptorType == vk::DescriptorType::eStorageBufferDynamic)
		{
			expectedDynamicOffsetCount += binding.descriptorCount;
		}
	}

	if(dynamicOffsets.size() == 0 && expectedDynamicOffsetCount > 0)
	{
		std::vector<uint32_t> zeros(expectedDynamicOffsetCount, 0u);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSets[0],
		                                expectedDynamicOffsetCount, zeros.data());
		return;
	}

	assert(dynamicOffsets.size() == expectedDynamicOffsetCount);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &descriptorSets[0],
	                                expectedDynamicOffsetCount, dynamicOffsets.begin());
}

vk::RenderPass DrawTester::createRenderPass(vk::Format colorFormat)
{
	std::vector<vk::AttachmentDescription> attachments(multisample ? 2 : 1);
	if(depthTestEnabled)
	{
		attachments.push_back(vk::AttachmentDescription{});
	}

	if(multisample)
	{
		// Color attachment
		attachments[0].format = colorFormat;
		attachments[0].samples = vk::SampleCountFlagBits::e4;
		attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eUndefined;
		attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		// Resolve attachment
		attachments[1].format = colorFormat;
		attachments[1].samples = vk::SampleCountFlagBits::e1;
		attachments[1].loadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[1].storeOp = vk::AttachmentStoreOp::eStore;
		attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].initialLayout = vk::ImageLayout::eUndefined;
		attachments[1].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	else
	{
		attachments[0].format = colorFormat;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = colorLoadOp;
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = (colorLoadOp == vk::AttachmentLoadOp::eLoad) ? vk::ImageLayout::ePresentSrcKHR : vk::ImageLayout::eUndefined;
		attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}

	vk::AttachmentReference attachment0;
	attachment0.attachment = 0;
	attachment0.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference attachment1;
	attachment1.attachment = 1;
	attachment1.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depthAttachment;
	depthAttachment.attachment = multisample ? 2 : 1;
	depthAttachment.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	if(depthTestEnabled)
	{
		auto &depth = attachments.back();
		depth.format = depthFormat;
		depth.samples = multisample ? vk::SampleCountFlagBits::e4 : vk::SampleCountFlagBits::e1;
		depth.loadOp = depthLoadOp;
		depth.storeOp = vk::AttachmentStoreOp::eDontCare;
		depth.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		depth.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		depth.initialLayout = (depthLoadOp == vk::AttachmentLoadOp::eLoad) ? vk::ImageLayout::eDepthStencilAttachmentOptimal : vk::ImageLayout::eUndefined;
		depth.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	}

	vk::SubpassDescription subpassDescriptions[2] = {};
	for(uint32_t subpassIndex = 0; subpassIndex < (secondSubpassEnabled ? 2u : 1u); subpassIndex++)
	{
		subpassDescriptions[subpassIndex].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpassDescriptions[subpassIndex].colorAttachmentCount = 1;
		subpassDescriptions[subpassIndex].pResolveAttachments = multisample ? &attachment1 : nullptr;
		subpassDescriptions[subpassIndex].pColorAttachments = &attachment0;
		subpassDescriptions[subpassIndex].pDepthStencilAttachment = depthTestEnabled ? &depthAttachment : nullptr;
	}

	std::vector<vk::SubpassDependency> dependencies;
	dependencies.resize(secondSubpassEnabled ? 3 : 2);

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependencies[0].dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
	dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

	if(secondSubpassEnabled)
	{
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = 1;
		dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		dependencies[1].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
		dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

		dependencies[2].srcSubpass = 1;
		dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	}
	else
	{
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	}
	dependencies.back().srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependencies.back().dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
	dependencies.back().srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
	dependencies.back().dstAccessMask = vk::AccessFlagBits::eMemoryRead;
	dependencies.back().dependencyFlags = vk::DependencyFlagBits::eByRegion;

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = secondSubpassEnabled ? 2u : 1u;
	renderPassInfo.pSubpasses = subpassDescriptions;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	return device.createRenderPass(renderPassInfo);
}

void DrawTester::createFramebuffers(vk::RenderPass renderPass)
{
	framebuffers.resize(swapchain->imageCount());

	for(size_t i = 0; i < framebuffers.size(); i++)
	{
		framebuffers[i].reset(new Framebuffer(device, physicalDevice, swapchain->getImageView(i), swapchain->colorFormat, renderPass, swapchain->getExtent(), multisample, depthTestEnabled, depthFormat));
	}
}

void DrawTester::prepareVertices()
{
	hooks.createVertexBuffers(*this);
}

vk::Pipeline DrawTester::createGraphicsPipeline(vk::RenderPass renderPass, uint32_t subpassIndex)
{
	auto setLayoutBindings = hooks.createDescriptorSetLayout(*this);
	descriptorSetLayoutBindings = setLayoutBindings;

	std::vector<vk::DescriptorSetLayout> setLayouts;
	if(!setLayoutBindings.empty())
	{
		vk::DescriptorSetLayoutCreateInfo layoutInfo;
		layoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		layoutInfo.pBindings = setLayoutBindings.data();
		descriptorSetLayout = device.createDescriptorSetLayout(layoutInfo);

		setLayouts.push_back(descriptorSetLayout);
	}

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
	pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

	vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;

	std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
	std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
	if(vertices.buffer)
	{
		bindingDescriptions.push_back(vertices.inputBinding);
		attributeDescriptions.insert(attributeDescriptions.end(), vertices.inputAttributes.begin(), vertices.inputAttributes.end());
	}
	if(instances.buffer)
	{
		bindingDescriptions.push_back(instances.inputBinding);
		attributeDescriptions.insert(attributeDescriptions.end(), instances.inputAttributes.begin(), instances.inputAttributes.end());
	}
	vk::PipelineVertexInputStateCreateInfo vertexInputState;
	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
	vertexInputState.pVertexBindingDescriptions = bindingDescriptions.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState;
	inputAssemblyState.topology = primitiveTopology;
	inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable ? VK_TRUE : VK_FALSE;

	vk::PipelineRasterizationStateCreateInfo rasterizationState;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.polygonMode = vk::PolygonMode::eFill;
	rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
	rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.lineWidth = lineWidth;

	vk::PipelineColorBlendAttachmentState blendAttachmentState;
	blendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	blendAttachmentState.blendEnable = VK_FALSE;
	vk::PipelineColorBlendStateCreateInfo colorBlendState;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	std::vector<vk::DynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(vk::DynamicState::eViewport);
	dynamicStateEnables.push_back(vk::DynamicState::eScissor);
	if(vertexInputDynamicStateEnabled)
	{
		dynamicStateEnables.push_back(vk::DynamicState::eVertexInputEXT);
	}
	vk::PipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

	vk::PipelineDepthStencilStateCreateInfo depthStencilState;
	depthStencilState.depthTestEnable = depthTestEnabled ? VK_TRUE : VK_FALSE;
	depthStencilState.depthWriteEnable = depthWriteEnabled ? VK_TRUE : VK_FALSE;
	depthStencilState.depthCompareOp = depthCompareOp;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.back.failOp = vk::StencilOp::eKeep;
	depthStencilState.back.passOp = vk::StencilOp::eKeep;
	depthStencilState.back.compareOp = vk::CompareOp::eAlways;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.front = depthStencilState.back;

	vk::PipelineMultisampleStateCreateInfo multisampleState;
	multisampleState.rasterizationSamples = multisample ? vk::SampleCountFlagBits::e4 : vk::SampleCountFlagBits::e1;
	multisampleState.pSampleMask = nullptr;

	vk::ShaderModule vertexModule = hooks.createVertexShader(*this);
	vk::ShaderModule fragmentModule = hooks.createFragmentShader(*this);

	assert(vertexModule);    // TODO: if nullptr, use a default
	assert(fragmentModule);  // TODO: if nullptr, use a default

	std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages;

	shaderStages[0].module = vertexModule;
	shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
	shaderStages[0].pName = "main";

	shaderStages[1].module = fragmentModule;
	shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
	shaderStages[1].pName = "main";

	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	vk::PipelineVertexInputStateCreateInfo emptyVertexInputState;
	pipelineCreateInfo.pVertexInputState = vertexInputDynamicStateEnabled ? &emptyVertexInputState : &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	vk::PipelineRenderingCreateInfo pipelineRenderingInfo;
	vk::Format colorAttachmentFormat = swapchain->colorFormat;
	if(dynamicRenderingEnabled)
	{
		assert(subpassIndex == 0);
		pipelineRenderingInfo.colorAttachmentCount = 1;
		pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
		pipelineCreateInfo.pNext = &pipelineRenderingInfo;
		pipelineCreateInfo.renderPass = vk::RenderPass{};
		pipelineCreateInfo.subpass = 0;
	}
	else
	{
		pipelineCreateInfo.pNext = nullptr;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.subpass = subpassIndex;
	}
	pipelineCreateInfo.pDynamicState = &dynamicState;

	auto pipeline = device.createGraphicsPipeline(nullptr, pipelineCreateInfo).value;

	device.destroyShaderModule(fragmentModule);
	device.destroyShaderModule(vertexModule);

	return pipeline;
}

void DrawTester::createSynchronizationPrimitives()
{
	vk::SemaphoreCreateInfo semaphoreCreateInfo;
	presentCompleteSemaphore = device.createSemaphore(semaphoreCreateInfo);
	renderCompleteSemaphore = device.createSemaphore(semaphoreCreateInfo);

	vk::FenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
	waitFences.resize(swapchain->imageCount());
	for(auto &fence : waitFences)
	{
		fence = device.createFence(fenceCreateInfo);
	}
}

void DrawTester::createCommandBuffers(vk::RenderPass renderPass)
{
	vk::CommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
	commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	commandPool = device.createCommandPool(commandPoolCreateInfo);

	descriptorSets.clear();
	if(descriptorSetLayout)
	{
		std::vector<vk::DescriptorPoolSize> poolSizes;
		poolSizes.reserve(descriptorSetLayoutBindings.size());
		for(const auto &binding : descriptorSetLayoutBindings)
		{
			if(binding.descriptorCount == 0)
			{
				continue;
			}

			auto found = std::find_if(poolSizes.begin(), poolSizes.end(), [&](const vk::DescriptorPoolSize &poolSize) {
				return poolSize.type == binding.descriptorType;
			});
			if(found == poolSizes.end())
			{
				poolSizes.emplace_back(binding.descriptorType, binding.descriptorCount);
			}
			else
			{
				found->descriptorCount += binding.descriptorCount;
			}
		}

		vk::DescriptorPoolCreateInfo poolInfo;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		descriptorPool = device.createDescriptorPool(poolInfo);

		std::vector<vk::DescriptorSetLayout> layouts(1, descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets = device.allocateDescriptorSets(allocInfo);

		hooks.updateDescriptorSet(*this, commandPool, descriptorSets[0]);
	}

	vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(swapchain->imageCount());
	commandBufferAllocateInfo.level = vk::CommandBufferLevel::ePrimary;

	commandBuffers = device.allocateCommandBuffers(commandBufferAllocateInfo);

		for(size_t i = 0; i < commandBuffers.size(); i++)
		{
			vk::CommandBufferBeginInfo commandBufferBeginInfo;
			commandBuffers[i].begin(commandBufferBeginInfo);

			if(dynamicRenderingEnabled)
			{
				assert(!multisample);
				assert(!secondSubpassEnabled);
				assert(!depthTestEnabled);

				vk::ImageMemoryBarrier colorBarrier;
				colorBarrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
				colorBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
				colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				colorBarrier.image = swapchain->getImage(i);
				colorBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				colorBarrier.subresourceRange.baseMipLevel = 0;
				colorBarrier.subresourceRange.levelCount = 1;
				colorBarrier.subresourceRange.baseArrayLayer = 0;
				colorBarrier.subresourceRange.layerCount = 1;
				colorBarrier.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
				colorBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

				commandBuffers[i].pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				                                  vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &colorBarrier);

				vk::RenderingAttachmentInfo colorAttachment;
				colorAttachment.imageView = swapchain->getImageView(i);
				colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
				colorAttachment.loadOp = colorLoadOp;
				colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
				colorAttachment.clearValue.color = vk::ClearColorValue(colorClearValue);

				vk::RenderingInfo renderingInfo;
				renderingInfo.renderArea.offset.x = 0;
				renderingInfo.renderArea.offset.y = 0;
				renderingInfo.renderArea.extent = windowSize;
				renderingInfo.layerCount = 1;
				renderingInfo.viewMask = 0;
				renderingInfo.colorAttachmentCount = 1;
				renderingInfo.pColorAttachments = &colorAttachment;

				commandBuffers[i].beginRendering(renderingInfo);
			}
			else
			{
				uint32_t clearValueCount = multisample ? (depthTestEnabled ? 3u : 2u) : (depthTestEnabled ? 2u : 1u);
				std::vector<vk::ClearValue> clearValues(clearValueCount);
				clearValues[0].color = vk::ClearColorValue(colorClearValue);
				if(depthTestEnabled)
				{
					clearValues[multisample ? 2u : 1u].depthStencil = vk::ClearDepthStencilValue(1.0f, 0u);
				}

				vk::RenderPassBeginInfo renderPassBeginInfo;
				renderPassBeginInfo.framebuffer = framebuffers[i]->getFramebuffer();
				renderPassBeginInfo.renderPass = renderPass;
				renderPassBeginInfo.renderArea.offset.x = 0;
				renderPassBeginInfo.renderArea.offset.y = 0;
				renderPassBeginInfo.renderArea.extent = windowSize;
				renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
				renderPassBeginInfo.pClearValues = clearValues.data();
				commandBuffers[i].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
			}

		// Set dynamic state
		vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(windowSize.width), static_cast<float>(windowSize.height), 0.0f, 1.0f);
		commandBuffers[i].setViewport(0, 1, &viewport);

		vk::Rect2D scissor(vk::Offset2D(0, 0), windowSize);
		commandBuffers[i].setScissor(0, 1, &scissor);

		if(!descriptorSets.empty())
		{
			bindDescriptorSet(commandBuffers[i]);
		}

		// Draw
		if(vertices.numVertices > 0)
		{
			auto bindCommonGraphicsState = [&](vk::Pipeline boundPipeline) {
				commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, boundPipeline);
				std::vector<vk::Buffer> vertexBuffers;
				std::vector<VULKAN_HPP_NAMESPACE::DeviceSize> offsets;
				vertexBuffers.push_back(vertices.buffer);
				offsets.push_back(0);
				if(instances.buffer)
				{
					vertexBuffers.push_back(instances.buffer);
					offsets.push_back(0);
				}
				commandBuffers[i].bindVertexBuffers(0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), offsets.data());
				if(vertexInputDynamicStateEnabled)
				{
					std::vector<vk::VertexInputBindingDescription2EXT> bindingDescriptions;
					std::vector<vk::VertexInputAttributeDescription2EXT> attributeDescriptions;
					auto appendBinding = [&](const VertexBuffer &buffer) {
						vk::VertexInputBindingDescription2EXT binding;
						binding.binding = buffer.inputBinding.binding;
						binding.stride = buffer.inputBinding.stride;
						binding.inputRate = buffer.inputBinding.inputRate;
						binding.divisor = 1;
						bindingDescriptions.push_back(binding);
						for(const auto &attribute : buffer.inputAttributes)
						{
							vk::VertexInputAttributeDescription2EXT attr;
							attr.location = attribute.location;
							attr.binding = attribute.binding;
							attr.format = attribute.format;
							attr.offset = attribute.offset;
							attributeDescriptions.push_back(attr);
						}
					};
					appendBinding(vertices);
					if(instances.buffer)
					{
						appendBinding(instances);
					}
					commandBuffers[i].setVertexInputEXT(bindingDescriptions, attributeDescriptions);
				}
				if(pushConstantEnabled)
				{
					commandBuffers[i].pushConstants(pipelineLayout, pushConstantStages, 0, pushConstantSize, pushConstantData.data());
				}
			};

			bindCommonGraphicsState(pipeline);
			if(hooks.recordDrawCommands)
			{
				hooks.recordDrawCommands(*this, commandBuffers[i]);
			}
			else
			{
				commandBuffers[i].draw(vertices.numVertices, 1, 0, 0);
			}

			if(secondSubpassEnabled)
			{
				commandBuffers[i].nextSubpass(vk::SubpassContents::eInline);
				bindCommonGraphicsState(secondSubpassPipeline);
				if(hooks.recordSecondSubpassCommands)
				{
					hooks.recordSecondSubpassCommands(*this, commandBuffers[i]);
				}
			}
		}

			if(dynamicRenderingEnabled)
			{
				commandBuffers[i].endRendering();

				vk::ImageMemoryBarrier presentBarrier;
				presentBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
				presentBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
				presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				presentBarrier.image = swapchain->getImage(i);
				presentBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				presentBarrier.subresourceRange.baseMipLevel = 0;
				presentBarrier.subresourceRange.levelCount = 1;
				presentBarrier.subresourceRange.baseArrayLayer = 0;
				presentBarrier.subresourceRange.layerCount = 1;
				presentBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				presentBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;

				commandBuffers[i].pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe,
				                                  vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &presentBarrier);
			}
			else
			{
				commandBuffers[i].endRenderPass();
			}
			commandBuffers[i].end();
		}
	}

void DrawTester::addVertexBuffer(void *vertexBufferData, size_t vertexBufferDataSize, size_t vertexSize, std::vector<vk::VertexInputAttributeDescription> inputAttributes)
{
	assert(!vertices.buffer);  // For now, only support adding once

	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.size = vertexBufferDataSize;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vertices.buffer = device.createBuffer(vertexBufferInfo);
	vertices.size = vertexBufferDataSize;

	vk::MemoryAllocateInfo memoryAllocateInfo;
	vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(vertices.buffer);
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = Util::getMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	vertices.memory = device.allocateMemory(memoryAllocateInfo);

	void *data = device.mapMemory(vertices.memory, 0, VK_WHOLE_SIZE);
	memcpy(data, vertexBufferData, vertexBufferDataSize);
	device.unmapMemory(vertices.memory);
	device.bindBufferMemory(vertices.buffer, vertices.memory, 0);

	vertices.inputBinding.binding = 0;
	vertices.inputBinding.stride = static_cast<uint32_t>(vertexSize);
	vertices.inputBinding.inputRate = vk::VertexInputRate::eVertex;

	vertices.inputAttributes = std::move(inputAttributes);

	vertices.inputState.vertexBindingDescriptionCount = 1;
	vertices.inputState.pVertexBindingDescriptions = &vertices.inputBinding;
	vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.inputAttributes.size());
	vertices.inputState.pVertexAttributeDescriptions = vertices.inputAttributes.data();

	// Note that we assume data is tightly packed
	vertices.numVertices = static_cast<uint32_t>(vertexBufferDataSize / vertexSize);
}

void DrawTester::addInstanceBuffer(void *vertexBufferData, size_t vertexBufferDataSize, size_t vertexSize, std::vector<vk::VertexInputAttributeDescription> inputAttributes)
{
	assert(!instances.buffer);

	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.size = vertexBufferDataSize;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	instances.buffer = device.createBuffer(vertexBufferInfo);
	instances.size = vertexBufferDataSize;

	vk::MemoryAllocateInfo memoryAllocateInfo;
	vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(instances.buffer);
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = Util::getMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	instances.memory = device.allocateMemory(memoryAllocateInfo);

	void *data = device.mapMemory(instances.memory, 0, VK_WHOLE_SIZE);
	memcpy(data, vertexBufferData, vertexBufferDataSize);
	device.unmapMemory(instances.memory);
	device.bindBufferMemory(instances.buffer, instances.memory, 0);

	instances.inputBinding.binding = 1;
	instances.inputBinding.stride = static_cast<uint32_t>(vertexSize);
	instances.inputBinding.inputRate = vk::VertexInputRate::eInstance;
	instances.inputAttributes = std::move(inputAttributes);
	instances.numVertices = static_cast<uint32_t>(vertexBufferDataSize / vertexSize);
}

void DrawTester::addIndexBuffer(void *indexBufferData, size_t indexBufferDataSize, vk::IndexType indexType)
{
	assert(!indices.buffer);

	vk::BufferCreateInfo indexBufferInfo;
	indexBufferInfo.size = indexBufferDataSize;
	indexBufferInfo.usage = vk::BufferUsageFlagBits::eIndexBuffer;
	indices.buffer = device.createBuffer(indexBufferInfo);
	indices.size = indexBufferDataSize;
	indices.type = indexType;

	vk::MemoryAllocateInfo memoryAllocateInfo;
	vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(indices.buffer);
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = Util::getMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	indices.memory = device.allocateMemory(memoryAllocateInfo);

	void *data = device.mapMemory(indices.memory, 0, VK_WHOLE_SIZE);
	memcpy(data, indexBufferData, indexBufferDataSize);
	device.unmapMemory(indices.memory);
	device.bindBufferMemory(indices.buffer, indices.memory, 0);
}

vk::ShaderModule DrawTester::createShaderModule(const char *glslSource, EShLanguage glslLanguage)
{
	auto spirv = Util::compileGLSLtoSPIRV(glslSource, glslLanguage);

	return createShaderModule(spirv);
}

vk::ShaderModule DrawTester::createShaderModule(const std::vector<uint32_t> &spirv)
{
	vk::ShaderModuleCreateInfo moduleCreateInfo;
	moduleCreateInfo.codeSize = spirv.size() * sizeof(uint32_t);
	moduleCreateInfo.pCode = (uint32_t *)spirv.data();

	return device.createShaderModule(moduleCreateInfo);
}
