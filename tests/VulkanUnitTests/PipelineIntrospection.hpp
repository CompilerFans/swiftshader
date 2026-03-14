#ifndef VK_UNITTESTS_PIPELINE_INTROSPECTION_HPP_
#define VK_UNITTESTS_PIPELINE_INTROSPECTION_HPP_

#include <cstdint>

enum class GraphicsPipelineBootstrapTextureResourceKind : uint32_t
{
	None = 0,
	CombinedImageSampler = 1,
	SeparateImageSampler = 2,
	Other = 3,
};

enum class GraphicsPipelineTriangleBootstrapUnsupportedReason : uint32_t
{
	None = 0,
	NoFragmentStage = 1u << 0,
	MissingBootstrapFragmentConfig = 1u << 1,
	TextureSamplingUnsupported = 1u << 2,
	StorageImageReadWrite = 1u << 3,
	ImageQueryOrFetch = 1u << 4,
	Derivatives = 1u << 5,
	Atomics = 1u << 6,
	Subgroup = 1u << 7,
	DiscardUnsupported = 1u << 8,
	BufferDescriptorsPresent = 1u << 9,
	NonConstantDescriptorArrayElement = 1u << 10,
};

struct GraphicsPipelineBootstrapFragmentState
{
	bool hasConfig = false;
	uint32_t shaderKind = 0;
	float colorR = 0.0f;
	float colorG = 0.0f;
	float colorB = 0.0f;
	float colorA = 0.0f;
};

struct GraphicsPipelineBootstrapTextureState
{
	bool hasPlan = false;
	bool hasBinding = false;
	uint32_t resourceKind = 0;
	uint32_t imageDescriptorSet = 0;
	uint32_t imageBinding = 0;
	uint32_t imageArrayElement = 0;
	uint32_t samplerDescriptorSet = 0;
	uint32_t samplerBinding = 0;
	uint32_t samplerArrayElement = 0;
};

struct GraphicsPipelineImageResourceState
{
	bool hasPlan = false;
	uint32_t sampledDescriptorCount = 0;
	uint32_t storageDescriptorCount = 0;
	uint32_t storageDescriptorSet = 0;
	uint32_t storageBinding = 0;
	uint32_t storageArrayElement = 0;
	uint32_t storageDescriptorType = 0;
};

struct GraphicsPipelineResourcePlanState
{
	bool hasPlan = false;
	uint32_t descriptorSetCount = 0;
	uint32_t dynamicOffsetCount = 0;
	uint32_t pushConstantSize = 0;
	uint32_t fragmentFeatureMask = 0;
	uint32_t triangleBootstrapUnsupportedReasonMask = 0;
	uint32_t descriptorRefCount = 0;
	uint32_t bufferDescriptorCount = 0;
	uint32_t firstBufferDescriptorSet = 0;
	uint32_t firstBufferBinding = 0;
	uint32_t firstBufferDescriptorType = 0;
	bool firstBufferIsDynamic = false;
	uint32_t firstBufferDynamicOffsetIndex = 0;
};

bool graphicsPipelineHasBackendExecutable(uintptr_t pipelineHandle);
float graphicsPipelineBootstrapPointSize(uintptr_t pipelineHandle);
GraphicsPipelineBootstrapFragmentState graphicsPipelineBootstrapFragmentState(uintptr_t pipelineHandle);
GraphicsPipelineBootstrapTextureState graphicsPipelineBootstrapTextureState(uintptr_t pipelineHandle);
GraphicsPipelineImageResourceState graphicsPipelineImageResourceState(uintptr_t pipelineHandle);
GraphicsPipelineResourcePlanState graphicsPipelineResourcePlanState(uintptr_t pipelineHandle);

#endif  // VK_UNITTESTS_PIPELINE_INTROSPECTION_HPP_
