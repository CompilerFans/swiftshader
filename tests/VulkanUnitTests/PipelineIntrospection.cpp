#include "PipelineIntrospection.hpp"

#include "Backend/GraphicsExecutable.hpp"
#include "Vulkan/VkPipeline.hpp"

namespace {

bool isBufferDescriptorType(VkDescriptorType descriptorType)
{
	switch(descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return true;
	default:
		return false;
	}
}

}  // namespace

bool graphicsPipelineHasBackendExecutable(uintptr_t pipelineHandle)
{
	auto *graphicsPipeline = reinterpret_cast<vk::GraphicsPipeline *>(pipelineHandle);
	return graphicsPipeline != nullptr && graphicsPipeline->hasBackendExecutable();
}

float graphicsPipelineBootstrapPointSize(uintptr_t pipelineHandle)
{
	auto *graphicsPipeline = reinterpret_cast<vk::GraphicsPipeline *>(pipelineHandle);
	if(graphicsPipeline == nullptr || graphicsPipeline->getBackendExecutable() == nullptr)
	{
		return 64.0f;
	}

	return graphicsPipeline->getBackendExecutable()->bootstrapPointSize();
}

GraphicsPipelineBootstrapFragmentState graphicsPipelineBootstrapFragmentState(uintptr_t pipelineHandle)
{
	GraphicsPipelineBootstrapFragmentState state = {};
	auto *graphicsPipeline = reinterpret_cast<vk::GraphicsPipeline *>(pipelineHandle);
	if(graphicsPipeline == nullptr || graphicsPipeline->getBackendExecutable() == nullptr)
	{
		return state;
	}

	const auto &executable = *graphicsPipeline->getBackendExecutable();
	if(!executable.hasBootstrapFragmentConfig())
	{
		return state;
	}

	const auto &config = executable.bootstrapFragmentConfig();
	state.hasConfig = true;
	state.shaderKind = static_cast<uint32_t>(config.shaderKind);
	state.colorR = config.colorR;
	state.colorG = config.colorG;
	state.colorB = config.colorB;
	state.colorA = config.colorA;
	return state;
}

GraphicsPipelineBootstrapTextureState graphicsPipelineBootstrapTextureState(uintptr_t pipelineHandle)
{
	GraphicsPipelineBootstrapTextureState state = {};
	auto *graphicsPipeline = reinterpret_cast<vk::GraphicsPipeline *>(pipelineHandle);
	if(graphicsPipeline == nullptr || graphicsPipeline->getBackendExecutable() == nullptr)
	{
		return state;
	}

	const auto &executable = *graphicsPipeline->getBackendExecutable();
	if(executable.hasTexturePlan())
	{
		const auto &plan = executable.texturePlan();
		state.hasPlan = true;
		state.resourceKind = static_cast<uint32_t>(plan.resourceKind);
		state.imageDescriptorSet = plan.imageDescriptorSet;
		state.imageBinding = plan.imageBinding;
		state.imageArrayElement = plan.imageArrayElement;
		state.samplerDescriptorSet = plan.samplerDescriptorSet;
		state.samplerBinding = plan.samplerBinding;
		state.samplerArrayElement = plan.samplerArrayElement;
	}

	if(!executable.hasBootstrapTextureBinding())
	{
		return state;
	}

	const auto binding = executable.bootstrapTextureBinding();
	state.hasBinding = true;
	state.imageDescriptorSet = binding.descriptorSet;
	state.imageBinding = binding.binding;
	state.imageArrayElement = state.hasPlan ? executable.texturePlan().imageArrayElement : 0u;
	state.samplerDescriptorSet = binding.descriptorSet;
	state.samplerBinding = binding.binding;
	state.samplerArrayElement = state.hasPlan ? executable.texturePlan().samplerArrayElement : 0u;
	return state;
}

GraphicsPipelineImageResourceState graphicsPipelineImageResourceState(uintptr_t pipelineHandle)
{
	GraphicsPipelineImageResourceState state = {};
	auto *graphicsPipeline = reinterpret_cast<vk::GraphicsPipeline *>(pipelineHandle);
	if(graphicsPipeline == nullptr || graphicsPipeline->getBackendExecutable() == nullptr)
	{
		return state;
	}

	const auto &executable = *graphicsPipeline->getBackendExecutable();
	if(!executable.hasImageResourcePlan())
	{
		return state;
	}

	const auto &plan = executable.imageResourcePlan();
	state.hasPlan = true;
	state.sampledDescriptorCount = static_cast<uint32_t>(plan.sampledDescriptors.size());
	state.storageDescriptorCount = static_cast<uint32_t>(plan.storageDescriptors.size());
	if(!plan.storageDescriptors.empty())
	{
		const auto &ref = plan.storageDescriptors[0];
		state.storageDescriptorSet = ref.descriptorSet;
		state.storageBinding = ref.binding;
		state.storageArrayElement = ref.arrayElement;
		state.storageDescriptorType = static_cast<uint32_t>(ref.descriptorType);
	}

	return state;
}

GraphicsPipelineResourcePlanState graphicsPipelineResourcePlanState(uintptr_t pipelineHandle)
{
	GraphicsPipelineResourcePlanState state = {};
	auto *graphicsPipeline = reinterpret_cast<vk::GraphicsPipeline *>(pipelineHandle);
	if(graphicsPipeline == nullptr || graphicsPipeline->getBackendExecutable() == nullptr)
	{
		return state;
	}

	const auto &executable = *graphicsPipeline->getBackendExecutable();
	if(!executable.hasResourcePlan())
	{
		return state;
	}

	const auto &plan = executable.resourcePlan();
	state.hasPlan = true;
	state.descriptorSetCount = plan.descriptorSetCount;
	state.dynamicOffsetCount = plan.dynamicOffsetCount;
	state.pushConstantSize = plan.pushConstantSize;
	state.fragmentFeatureMask = executable.fragmentFeatureMask();
	state.triangleBootstrapUnsupportedReasonMask = executable.triangleBootstrapUnsupportedReasonMask();
	state.descriptorRefCount = static_cast<uint32_t>(plan.descriptors.size());

	bool sawFirstBuffer = false;
	uint32_t bufferCount = 0;
	for(const auto &ref : plan.descriptors)
	{
		if(!isBufferDescriptorType(ref.descriptorType))
		{
			continue;
		}

		bufferCount++;
		if(!sawFirstBuffer)
		{
			state.firstBufferDescriptorSet = ref.descriptorSet;
			state.firstBufferBinding = ref.binding;
			state.firstBufferDescriptorType = static_cast<uint32_t>(ref.descriptorType);
			state.firstBufferIsDynamic = ref.isDynamic;
			state.firstBufferDynamicOffsetIndex = ref.dynamicOffsetIndex;
			sawFirstBuffer = true;
		}
	}

	state.bufferDescriptorCount = bufferCount;
	return state;
}
