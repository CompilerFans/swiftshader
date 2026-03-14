#include "TriangleBootstrapDraw.hpp"

#include "Backend/FragmentBootstrap.hpp"
#include "Backend/GraphicsExecutable.hpp"
#include "Backend/TrianglePipelineBootstrap.hpp"
#include "System/Debug.hpp"
#include "Vulkan/VkDescriptorSet.hpp"
#include "Vulkan/VkDevice.hpp"
#include "Vulkan/VkPipeline.hpp"
#include "Vulkan/VkPipelineLayout.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace backend {
namespace {

bool envVarEnabled(const char *name)
{
	const char *value = std::getenv(name);
	return value != nullptr && value[0] != '\0';
}

bool shouldRenderTriangleBootstrapToColorAttachment()
{
	return envVarEnabled("SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP");
}

bool shouldRequireTriangleBootstrap()
{
	return envVarEnabled("SWIFTSHADER_GPU_REQUIRE_TRIANGLE_BOOTSTRAP");
}

bool shouldTraceTriangleBootstrapRender()
{
	return envVarEnabled("SWIFTSHADER_GPU_TRACE_TRIANGLE_BOOTSTRAP_RENDER");
}

bool shouldAllowCpuFallback()
{
	return envVarEnabled("SWIFTSHADER_GPU_ALLOW_CPU_FALLBACK");
}

std::string formatTriangleBootstrapUnsupportedReasons(uint32_t mask)
{
	std::string out;
	auto append = [&](const char *name) {
		if(!out.empty())
		{
			out += ", ";
		}
		out += name;
	};

	using Reason = GraphicsExecutableTriangleBootstrapUnsupportedReason;

	if((mask & static_cast<uint32_t>(Reason::NoFragmentStage)) != 0) { append("NoFragmentStage"); }
	if((mask & static_cast<uint32_t>(Reason::MissingBootstrapFragmentConfig)) != 0) { append("MissingBootstrapFragmentConfig"); }
	if((mask & static_cast<uint32_t>(Reason::TextureSamplingUnsupported)) != 0) { append("TextureSamplingUnsupported"); }
	if((mask & static_cast<uint32_t>(Reason::StorageImageReadWrite)) != 0) { append("StorageImageReadWrite"); }
	if((mask & static_cast<uint32_t>(Reason::ImageQueryOrFetch)) != 0) { append("ImageQueryOrFetch"); }
	if((mask & static_cast<uint32_t>(Reason::Derivatives)) != 0) { append("Derivatives"); }
	if((mask & static_cast<uint32_t>(Reason::Atomics)) != 0) { append("Atomics"); }
	if((mask & static_cast<uint32_t>(Reason::Subgroup)) != 0) { append("Subgroup"); }
	if((mask & static_cast<uint32_t>(Reason::DiscardUnsupported)) != 0) { append("DiscardUnsupported"); }
	if((mask & static_cast<uint32_t>(Reason::BufferDescriptorsPresent)) != 0) { append("BufferDescriptorsPresent"); }
	if((mask & static_cast<uint32_t>(Reason::NonConstantDescriptorArrayElement)) != 0) { append("NonConstantDescriptorArrayElement"); }

	if(out.empty())
	{
		append("Unknown");
	}

	return out;
}

bool writeTriangleBootstrapColorToAttachment(const std::vector<uint8_t> &colorBuffer, uint32_t width, uint32_t height, vk::ImageView *colorAttachment, const VkRect2D &renderArea, int layer)
{
	if(!colorAttachment || width == 0 || height == 0 || colorBuffer.empty())
	{
		return false;
	}
	if(renderArea.offset.x < 0 || renderArea.offset.y < 0)
	{
		return false;
	}
	if(colorAttachment->getSampleCount() != 1)
	{
		return false;
	}

	const size_t expectedBytes = static_cast<size_t>(width) * height * 4u;
	if(colorBuffer.size() < expectedBytes)
	{
		return false;
	}

	const VkFormat format = colorAttachment->getFormat(VK_IMAGE_ASPECT_COLOR_BIT);
	const bool isRgba = (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB);
	const bool isBgra = (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB);
	if(!isRgba && !isBgra)
	{
		return false;
	}

	const uint32_t rowPitchBytes = colorAttachment->rowPitchBytes(VK_IMAGE_ASPECT_COLOR_BIT, 0);
	if(rowPitchBytes < width * 4u)
	{
		return false;
	}

	uint8_t *dstBase = static_cast<uint8_t *>(colorAttachment->getOffsetPointer({ renderArea.offset.x, renderArea.offset.y, 0 }, VK_IMAGE_ASPECT_COLOR_BIT, 0, layer));
	const uint8_t *srcBase = colorBuffer.data();
	for(uint32_t y = 0; y < height; y++)
	{
		uint8_t *dstRow = dstBase + static_cast<size_t>(y) * rowPitchBytes;
		const uint8_t *srcRow = srcBase + static_cast<size_t>(y) * width * 4u;
		for(uint32_t x = 0; x < width; x++)
		{
			const uint8_t srcA = srcRow[x * 4u + 3u];
			if(srcA == 0u)
			{
				continue;
			}
			if(isRgba)
			{
				std::memcpy(dstRow + x * 4u, srcRow + x * 4u, 4u);
			}
			else
			{
				dstRow[x * 4u + 0u] = srcRow[x * 4u + 2u];
				dstRow[x * 4u + 1u] = srcRow[x * 4u + 1u];
				dstRow[x * 4u + 2u] = srcRow[x * 4u + 0u];
				dstRow[x * 4u + 3u] = srcA;
			}
		}
	}

	colorAttachment->contentsChanged(vk::Image::DIRECT_MEMORY_ACCESS);
	return true;
}

bool tryGetSampledImageDescriptor(uint32_t descriptorSetIndex,
                                  uint32_t bindingIndex,
                                  uint32_t arrayElement,
                                  VkDescriptorType expectedDescriptorType,
                                  const vk::PipelineLayout *pipelineLayout,
                                  const vk::DescriptorSet::Bindings &descriptorSets,
                                  const vk::SampledImageDescriptor **descriptor)
{
	if(descriptor == nullptr || pipelineLayout == nullptr)
	{
		return false;
	}

	if(descriptorSetIndex >= pipelineLayout->getDescriptorSetCount())
	{
		return false;
	}
	if(bindingIndex >= pipelineLayout->getBindingCount(descriptorSetIndex))
	{
		return false;
	}
	if(pipelineLayout->getDescriptorType(descriptorSetIndex, bindingIndex) != expectedDescriptorType)
	{
		return false;
	}
	if(arrayElement >= pipelineLayout->getDescriptorCount(descriptorSetIndex, bindingIndex))
	{
		return false;
	}

	auto descriptorSet = descriptorSets[descriptorSetIndex];
	if(descriptorSet == nullptr)
	{
		return false;
	}

	const auto bindingOffset = pipelineLayout->getBindingOffset(descriptorSetIndex, bindingIndex);
	const auto descriptorSize = pipelineLayout->getDescriptorSize(descriptorSetIndex, bindingIndex);
	const auto elementOffset = bindingOffset + arrayElement * descriptorSize;
	auto *sampledImage = reinterpret_cast<const vk::SampledImageDescriptor *>(descriptorSet + elementOffset);
	if(sampledImage == nullptr)
	{
		return false;
	}

	*descriptor = sampledImage;
	return true;
}

bool tryBuildBootstrapTextureConfig(const GraphicsExecutableTexturePlan &texturePlan,
                                   const vk::PipelineLayout *pipelineLayout,
                                   const vk::DescriptorSet::Bindings &descriptorSets,
                                   vk::Device *device,
                                   FragmentBootstrapConfig *config)
{
	if(config == nullptr || pipelineLayout == nullptr || device == nullptr)
	{
		return false;
	}

	const vk::SampledImageDescriptor *imageDescriptor = nullptr;
	uint32_t samplerId = 0;

	switch(texturePlan.resourceKind)
	{
	case GraphicsExecutableTextureResourceKind::CombinedImageSampler:
		if(!tryGetSampledImageDescriptor(texturePlan.imageDescriptorSet,
		                                texturePlan.imageBinding,
		                                texturePlan.imageArrayElement,
		                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		                                pipelineLayout,
		                                descriptorSets,
		                                &imageDescriptor))
		{
			return false;
		}
		samplerId = imageDescriptor->samplerId;
		break;
	case GraphicsExecutableTextureResourceKind::SeparateImageSampler:
		{
			const vk::SampledImageDescriptor *samplerDescriptor = nullptr;
			if(!tryGetSampledImageDescriptor(texturePlan.imageDescriptorSet,
			                                texturePlan.imageBinding,
			                                texturePlan.imageArrayElement,
			                                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			                                pipelineLayout,
			                                descriptorSets,
			                                &imageDescriptor))
			{
				return false;
			}
			if(!tryGetSampledImageDescriptor(texturePlan.samplerDescriptorSet,
			                                texturePlan.samplerBinding,
			                                texturePlan.samplerArrayElement,
			                                VK_DESCRIPTOR_TYPE_SAMPLER,
			                                pipelineLayout,
			                                descriptorSets,
			                                &samplerDescriptor))
			{
				return false;
			}
			samplerId = samplerDescriptor->samplerId;
		}
		break;
	default:
		return false;
	}

	if(imageDescriptor == nullptr || imageDescriptor->memoryOwner == nullptr || imageDescriptor->texture.mipmap[0].buffer == nullptr)
	{
		return false;
	}

	auto format = imageDescriptor->memoryOwner->getFormat(vk::ImageView::SAMPLING);
	if(format.bytes() != 4 || imageDescriptor->width <= 0 || imageDescriptor->height <= 0)
	{
		return false;
	}

	const vk::SamplerState *samplerState = device->findSampler(samplerId);
	if(!samplerState)
	{
		return false;
	}
	if(samplerState->addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE && samplerState->addressModeU != VK_SAMPLER_ADDRESS_MODE_REPEAT)
	{
		return false;
	}
	if(samplerState->addressModeV != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE && samplerState->addressModeV != VK_SAMPLER_ADDRESS_MODE_REPEAT)
	{
		return false;
	}

	config->shaderKind = FragmentBootstrapShaderKind::Texture2DColor;
	config->textureWidth = static_cast<uint32_t>(imageDescriptor->width);
	config->textureHeight = static_cast<uint32_t>(imageDescriptor->height);
	config->textureRowPitchTexels = imageDescriptor->texture.mipmap[0].pitchP.x;
	config->textureFilterLinear = (samplerState->magFilter == VK_FILTER_LINEAR || samplerState->minFilter == VK_FILTER_LINEAR) ? 1u : 0u;
	config->textureAddressModeU = samplerState->addressModeU == VK_SAMPLER_ADDRESS_MODE_REPEAT ? 1u : 0u;
	config->textureAddressModeV = samplerState->addressModeV == VK_SAMPLER_ADDRESS_MODE_REPEAT ? 1u : 0u;
	size_t textureBytes = static_cast<size_t>(config->textureRowPitchTexels) * config->textureHeight * 4u;
	auto *bytes = reinterpret_cast<const uint8_t *>(imageDescriptor->texture.mipmap[0].buffer);
	config->textureData.assign(bytes, bytes + textureBytes);
	return true;
}

struct TriangleBootstrapInvocationConfig
{
	FragmentBootstrapConfig fragmentConfig = {};
	const FragmentBootstrapConfig *fragmentConfigPtr = nullptr;
	const sw::Stream *colorStream = nullptr;
	const sw::Stream *texCoordStream = nullptr;
	bool vertexPushConstantOffsetEnabled = false;
	GraphicsBootstrapRuntimeConfig vertexRuntimeConfig = {};
	float pointSize = 64.0f;
};

TriangleBootstrapInvocationConfig buildTriangleBootstrapInvocationConfig(const vk::GraphicsPipeline &pipeline,
                                                                        const vk::PreRasterizationState &preRasterizationState,
                                                                        const vk::FragmentState *fragmentState,
                                                                        const vk::Inputs &inputs,
                                                                        vk::Device *device)
{
	TriangleBootstrapInvocationConfig config = {};
	if(const GraphicsExecutable *executable = pipeline.getBackendExecutable())
	{
		config.pointSize = executable->bootstrapPointSize();
		if(executable->hasBootstrapFragmentConfig())
		{
			config.fragmentConfig = executable->bootstrapFragmentConfig();
			config.fragmentConfigPtr = &config.fragmentConfig;
		}
		if(fragmentState && executable->hasTexturePlan() && executable->texturePlan().bootstrapSupported)
		{
			const auto originalShaderKind = config.fragmentConfig.shaderKind;
			if(fragmentState->getPipelineLayout() != preRasterizationState.getPipelineLayout())
			{
				vk::DescriptorSet::PrepareForSampling(inputs.getDescriptorSetObjects(), fragmentState->getPipelineLayout(), device);
			}
			if(tryBuildBootstrapTextureConfig(executable->texturePlan(),
			                                 fragmentState->getPipelineLayout(),
			                                 inputs.getDescriptorSets(),
			                                 device,
			                                 &config.fragmentConfig))
			{
				config.fragmentConfig.shaderKind = originalShaderKind;
				config.fragmentConfigPtr = &config.fragmentConfig;
			}
		}
	}
	config.vertexPushConstantOffsetEnabled =
	    preRasterizationState.getPipelineLayout() &&
	    preRasterizationState.getPipelineLayout()->hasPushConstantStage(VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2u);

	if(config.fragmentConfigPtr && config.fragmentConfig.shaderKind == FragmentBootstrapShaderKind::Texture2DColor)
	{
		if(inputs.getStream(1).format != VK_FORMAT_UNDEFINED)
		{
			config.texCoordStream = &inputs.getStream(1);
		}
	}
	else if(config.fragmentConfigPtr && config.fragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
	{
		if(inputs.getStream(1).format != VK_FORMAT_UNDEFINED)
		{
			config.colorStream = &inputs.getStream(1);
		}
		if(inputs.getStream(2).format != VK_FORMAT_UNDEFINED)
		{
			config.texCoordStream = &inputs.getStream(2);
		}
	}
	else if(inputs.getStream(1).format != VK_FORMAT_UNDEFINED)
	{
		config.colorStream = &inputs.getStream(1);
	}

	return config;
}

}  // namespace

bool tryTriangleBootstrapDraw(vk::Device *device,
                              RuntimeAPI &runtime,
                              const GraphicsDrawCall &draw,
                              bool *bootstrapAlreadyDone)
{
	if(device == nullptr || draw.pipeline == nullptr || draw.dynamicState == nullptr)
	{
		return false;
	}

	const vk::GraphicsState &pipelineState = draw.pipeline->getCombinedState(*draw.dynamicState);
	const vk::PreRasterizationState &preRasterizationState = pipelineState.getPreRasterizationState();
	const bool hasRasterizerDiscard = preRasterizationState.hasRasterizerDiscard();
	const bool renderTriangleBootstrap = shouldRenderTriangleBootstrapToColorAttachment();
	const GraphicsDrawRoute route = chooseGraphicsDrawRoute(true, runtime.isHardwareBacked(), shouldAllowCpuFallback(), renderTriangleBootstrap, hasRasterizerDiscard);
	const TriangleBootstrapPlan plan = planTriangleBootstrapDraw(route,
	                                                            renderTriangleBootstrap,
	                                                            shouldRequireTriangleBootstrap(),
	                                                            bootstrapAlreadyDone && *bootstrapAlreadyDone);
	if(plan.pass == TriangleBootstrapPass::Skip)
	{
		return false;
	}

	vk::ImageView *colorAttachment = nullptr;
	bool requiresTextureFragmentConfig = false;
	if(plan.pass == TriangleBootstrapPass::RenderToColorAttachment)
	{
		const GraphicsExecutable *executable = draw.pipeline->getBackendExecutable();
		if(executable == nullptr)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: missing backend executable\n");
			}
			return false;
		}

		const uint32_t unsupportedMask = executable->triangleBootstrapUnsupportedReasonMask();
		if(unsupportedMask != 0)
		{
			if(plan.requireSuccessfulWriteback)
			{
				const std::string reasons = formatTriangleBootstrapUnsupportedReasons(unsupportedMask);
				sw::abort("triangle bootstrap unsupported: %s\n", reasons.c_str());
			}
			return false;
		}

		requiresTextureFragmentConfig = executable->hasTexturePlan() && executable->texturePlan().bootstrapSupported;

		colorAttachment = draw.colorAttachment0.imageView;
		if(!draw.colorAttachment0.present || colorAttachment == nullptr)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: missing color attachment 0\n");
			}
			return false;
		}
		if(draw.colorAttachment0.storeOp != VK_ATTACHMENT_STORE_OP_STORE)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: color attachment storeOp must be STORE\n");
			}
			return false;
		}
		if(!canWriteTriangleBootstrapColorAttachment(draw.colorAttachment0))
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: color attachment layout must allow write-back\n");
			}
			return false;
		}
		if(colorAttachment->getSampleCount() != 1)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: color attachment must be single-sampled\n");
			}
			return false;
		}

		const VkFormat format = colorAttachment->getFormat(VK_IMAGE_ASPECT_COLOR_BIT);
		const bool isRgba = (format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB);
		const bool isBgra = (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB);
		if(!isRgba && !isBgra)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: unsupported color attachment format\n");
			}
			return false;
		}
	}

	const vk::FragmentState *fragmentState = hasRasterizerDiscard ? nullptr : &pipelineState.getFragmentState();
	const vk::Inputs &inputs = draw.pipeline->getInputs();
	vk::DescriptorSet::PrepareForSampling(inputs.getDescriptorSetObjects(), preRasterizationState.getPipelineLayout(), device);

	TriangleBootstrapInvocationConfig config = buildTriangleBootstrapInvocationConfig(*draw.pipeline,
	                                                                                 preRasterizationState,
	                                                                                 fragmentState,
	                                                                                 inputs,
	                                                                                 device);
	if(config.vertexPushConstantOffsetEnabled && draw.pushConstants != nullptr)
	{
		const auto *pushConstants = reinterpret_cast<const vk::Pipeline::PushConstantStorage *>(draw.pushConstants);
		config.vertexRuntimeConfig.offsetX = *reinterpret_cast<const float *>(&pushConstants->data[0]);
		config.vertexRuntimeConfig.offsetY = *reinterpret_cast<const float *>(&pushConstants->data[4]);
	}
	if(plan.pass == TriangleBootstrapPass::RenderToColorAttachment)
	{
		if(config.fragmentConfigPtr == nullptr)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: could not materialize fragment config\n");
			}
			return false;
		}
		if(requiresTextureFragmentConfig &&
		   config.fragmentConfigPtr->shaderKind != FragmentBootstrapShaderKind::Texture2DColor &&
		   config.fragmentConfigPtr->shaderKind != FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: could not materialize texture fragment config\n");
			}
			return false;
		}
		if(config.fragmentConfigPtr->shaderKind == FragmentBootstrapShaderKind::Texture2DColor && config.texCoordStream == nullptr)
		{
			if(plan.requireSuccessfulWriteback)
			{
				sw::abort("triangle bootstrap unsupported: missing texcoord stream for texture config\n");
			}
			return false;
		}
	}
	const sw::Stream &positionStream = inputs.getStream(0);
	const VkPrimitiveTopology topology = pipelineState.getVertexInputInterfaceState().getTopology();
	const VkIndexType indexType = draw.indexBuffer ? draw.pipeline->getIndexBuffer().getIndexType() : VK_INDEX_TYPE_UINT16;
	const bool frontFaceCounterClockwise = preRasterizationState.getFrontFace() == VK_FRONT_FACE_COUNTER_CLOCKWISE;

	if(plan.pass == TriangleBootstrapPass::WarmupOnly)
	{
		bool bootstrapSucceeded = false;
		if(positionStream.buffer && positionStream.format != VK_FORMAT_UNDEFINED)
		{
			TrianglePipelineBootstrapConfig bootstrapConfig = {};
			if(!buildTrianglePipelineBootstrapConfig(positionStream, config.colorStream, topology, draw.count, draw.renderArea, &bootstrapConfig, config.fragmentConfigPtr, draw.indexBuffer, indexType, draw.baseVertex, frontFaceCounterClockwise, config.pointSize, config.texCoordStream))
			{
				bootstrapSucceeded = false;
			}
			else
			{
				bootstrapConfig.runtimeConfig = config.vertexRuntimeConfig;
				bootstrapSucceeded = runTrianglePipelineBootstrap(runtime, bootstrapConfig, nullptr);
			}
		}
		else
		{
			bootstrapSucceeded = runTrianglePipelineBootstrap(runtime, 64u, 64u, nullptr);
		}

		if(runtime.isHardwareBacked() && bootstrapSucceeded && bootstrapAlreadyDone)
		{
			*bootstrapAlreadyDone = true;
		}
		return false;
	}

	std::vector<uint8_t> bootstrapColorBuffer;
	bool rendered = false;
	if(positionStream.buffer && positionStream.format != VK_FORMAT_UNDEFINED)
	{
		TrianglePipelineBootstrapConfig bootstrapConfig = {};
		if(!buildTrianglePipelineBootstrapConfig(positionStream, config.colorStream, topology, draw.count, draw.renderArea, &bootstrapConfig, config.fragmentConfigPtr, draw.indexBuffer, indexType, draw.baseVertex, frontFaceCounterClockwise, config.pointSize, config.texCoordStream))
		{
			rendered = false;
		}
		else
		{
			bootstrapConfig.runtimeConfig = config.vertexRuntimeConfig;
			rendered = runTrianglePipelineBootstrap(runtime, bootstrapConfig, &bootstrapColorBuffer);
		}
	}
	else
	{
		rendered = runTrianglePipelineBootstrap(runtime, draw.renderArea.extent.width, draw.renderArea.extent.height, &bootstrapColorBuffer);
	}

	const bool wrote = rendered && runtime.isHardwareBacked() && writeTriangleBootstrapColorToAttachment(bootstrapColorBuffer, draw.renderArea.extent.width, draw.renderArea.extent.height, colorAttachment, draw.renderArea, draw.layer);
	if(shouldTraceTriangleBootstrapRender())
	{
		std::fprintf(stderr,
		             "[gpu] triangle bootstrap render: rendered=%d wrote=%d pushPtr=%p pushOffsetEnabled=%d vertexOffset=(%f,%f,%f)\n",
		             rendered ? 1 : 0,
		             wrote ? 1 : 0,
		             draw.pushConstants,
		             config.vertexPushConstantOffsetEnabled ? 1 : 0,
		             config.vertexRuntimeConfig.offsetX,
		             config.vertexRuntimeConfig.offsetY,
		             config.vertexRuntimeConfig.offsetZ);
	}
	if(wrote)
	{
		return true;
	}
	if(plan.requireSuccessfulWriteback)
	{
		sw::abort("SWIFTSHADER_GPU_RENDER_TRIANGLE_BOOTSTRAP failed (rendered=%d, wrote=%d)\n", rendered ? 1 : 0, wrote ? 1 : 0);
	}
	return false;
}

}  // namespace backend
