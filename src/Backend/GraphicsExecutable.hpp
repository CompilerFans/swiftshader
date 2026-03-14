#ifndef SWIFTSHADER_GRAPHICS_EXECUTABLE_HPP_
#define SWIFTSHADER_GRAPHICS_EXECUTABLE_HPP_

#include "Backend/FragmentBootstrap.hpp"
#include "Pipeline/VertexLoweringInfo.hpp"
#include "Vulkan/VkConfig.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sw {
class SemanticIRModule;
class SpirvShader;
}

namespace backend {

struct GraphicsExecutableTextureBinding
{
	uint32_t descriptorSet = 0;
	uint32_t binding = 0;
};

enum class GraphicsExecutableTextureResourceKind : uint32_t
{
	None = 0,
	CombinedImageSampler = 1,
	SeparateImageSampler = 2,
	Other = 3,
};

enum class GraphicsExecutableFragmentFeature : uint32_t
{
	None = 0,
	Discard = 1u << 0,
	StorageImageReadWrite = 1u << 1,
	ImageQueryOrFetch = 1u << 2,
	Derivatives = 1u << 3,
	Atomics = 1u << 4,
	Subgroup = 1u << 5,
};

// Reasons why the current triangle-bootstrap render path cannot produce correct results for a pipeline.
// This is intentionally conservative: it is used to avoid silent wrong output in strict GPU mode, and
// to produce actionable fallbacks when CPU fallback is enabled.
enum class GraphicsExecutableTriangleBootstrapUnsupportedReason : uint32_t
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

struct GraphicsExecutableTexturePlan
{
	GraphicsExecutableTextureResourceKind resourceKind = GraphicsExecutableTextureResourceKind::None;
	uint32_t imageDescriptorSet = 0;
	uint32_t imageBinding = 0;
	uint32_t imageArrayElement = 0;
	uint32_t samplerDescriptorSet = 0;
	uint32_t samplerBinding = 0;
	uint32_t samplerArrayElement = 0;
	bool bootstrapSupported = false;
};

struct GraphicsExecutableDescriptorRef
{
	VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	uint32_t descriptorCount = 0;
	uint32_t descriptorSet = 0;
	uint32_t binding = 0;
	uint32_t arrayElement = 0;
	bool hasNonConstantArrayElement = false;
	bool isDynamic = false;
	uint32_t dynamicOffsetIndex = 0;
};

struct GraphicsExecutableImageResourcePlan
{
	std::vector<GraphicsExecutableDescriptorRef> sampledDescriptors;
	std::vector<GraphicsExecutableDescriptorRef> storageDescriptors;
};

struct GraphicsExecutableResourcePlan
{
	uint32_t descriptorSetCount = 0;
	uint32_t dynamicOffsetCount = 0;
	uint32_t pushConstantSize = 0;
	std::vector<GraphicsExecutableDescriptorRef> descriptors;
};

struct GraphicsExecutableDescriptorBindingInfo
{
	VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	uint32_t descriptorCount = 0;
	bool isDynamic = false;
	uint32_t dynamicOffsetIndex = 0;
};

using GraphicsExecutableQueryDescriptorBindingInfo = bool (*)(const void *userdata,
                                                              uint32_t descriptorSet,
                                                              uint32_t binding,
                                                              GraphicsExecutableDescriptorBindingInfo *bindingInfo);

struct GraphicsExecutableCreateInfo
{
	std::shared_ptr<sw::SemanticIRModule> vertexModule;
	std::shared_ptr<sw::SemanticIRModule> fragmentModule;
	const sw::SpirvShader *vertexShader = nullptr;
	const sw::SpirvShader *fragmentShader = nullptr;
	bool vertexPushConstantOffsetRuntimeSupported = false;
	GraphicsExecutableQueryDescriptorBindingInfo queryDescriptorBindingInfo = nullptr;
	const void *queryDescriptorBindingInfoUserdata = nullptr;
	uint32_t descriptorSetCount = 0;
	uint32_t dynamicOffsetCount = 0;
	uint32_t pushConstantSize = vk::MAX_PUSH_CONSTANT_SIZE;
};

class GraphicsExecutable
{
public:
	static std::shared_ptr<GraphicsExecutable> create(const GraphicsExecutableCreateInfo &createInfo);
	static std::shared_ptr<GraphicsExecutable> create(const std::shared_ptr<sw::SemanticIRModule> &vertexModule,
	                                                  const std::shared_ptr<sw::SemanticIRModule> &fragmentModule);

	bool valid() const { return !vertexStageEntryPoint.empty(); }
	const std::string &vertexEntryPoint() const { return vertexStageEntryPoint; }
	bool hasFragmentStage() const { return !fragmentStageEntryPoint.empty(); }
	const std::string &fragmentEntryPoint() const { return fragmentStageEntryPoint; }
	const sw::VertexLoweringInfo &vertexLowering() const { return vertexStageLowering; }
	bool hasBootstrapVertexPushConstantOffset() const { return bootstrapVertexPushConstantOffsetValid; }
	float bootstrapPointSize() const { return bootstrapPointSizeValue; }
	bool hasBootstrapFragmentConfig() const { return bootstrapFragmentConfigValid; }
	const FragmentBootstrapConfig &bootstrapFragmentConfig() const { return bootstrapFragmentConfigValue; }
	bool hasTexturePlan() const { return texturePlanValid; }
	const GraphicsExecutableTexturePlan &texturePlan() const { return texturePlanValue; }
	bool hasBootstrapTextureBinding() const { return texturePlanValid && texturePlanValue.bootstrapSupported && texturePlanValue.resourceKind == GraphicsExecutableTextureResourceKind::CombinedImageSampler; }
	GraphicsExecutableTextureBinding bootstrapTextureBinding() const { return { texturePlanValue.imageDescriptorSet, texturePlanValue.imageBinding }; }
	bool hasImageResourcePlan() const { return imageResourcePlanValid; }
	const GraphicsExecutableImageResourcePlan &imageResourcePlan() const { return imageResourcePlanValue; }
	bool hasResourcePlan() const { return resourcePlanValid; }
	const GraphicsExecutableResourcePlan &resourcePlan() const { return resourcePlanValue; }
	uint32_t fragmentFeatureMask() const { return fragmentFeatureMaskValue; }
	uint32_t triangleBootstrapUnsupportedReasonMask() const { return triangleBootstrapUnsupportedReasonMaskValue; }

private:
	GraphicsExecutable(std::string vertexEntryPoint, std::string fragmentEntryPoint,
	                   sw::VertexLoweringInfo vertexLowering,
	                   bool bootstrapVertexPushConstantOffsetValid,
	                   float bootstrapPointSize,
	                   bool bootstrapFragmentConfigValid,
	                   FragmentBootstrapConfig bootstrapFragmentConfig,
	                   bool texturePlanValid,
	                   GraphicsExecutableTexturePlan texturePlan,
	                   bool imageResourcePlanValid,
	                   GraphicsExecutableImageResourcePlan imageResourcePlan,
	                   bool resourcePlanValid,
	                   GraphicsExecutableResourcePlan resourcePlan,
	                   uint32_t fragmentFeatureMask,
	                   uint32_t triangleBootstrapUnsupportedReasonMask)
	    : vertexStageEntryPoint(std::move(vertexEntryPoint))
	    , fragmentStageEntryPoint(std::move(fragmentEntryPoint))
	    , vertexStageLowering(std::move(vertexLowering))
	    , bootstrapVertexPushConstantOffsetValid(bootstrapVertexPushConstantOffsetValid)
	    , bootstrapPointSizeValue(bootstrapPointSize)
	    , bootstrapFragmentConfigValid(bootstrapFragmentConfigValid)
	    , bootstrapFragmentConfigValue(std::move(bootstrapFragmentConfig))
	    , texturePlanValid(texturePlanValid)
	    , texturePlanValue(std::move(texturePlan))
	    , imageResourcePlanValid(imageResourcePlanValid)
	    , imageResourcePlanValue(std::move(imageResourcePlan))
	    , resourcePlanValid(resourcePlanValid)
	    , resourcePlanValue(std::move(resourcePlan))
	    , fragmentFeatureMaskValue(fragmentFeatureMask)
	    , triangleBootstrapUnsupportedReasonMaskValue(triangleBootstrapUnsupportedReasonMask)
	{}

	std::string vertexStageEntryPoint;
	std::string fragmentStageEntryPoint;
	sw::VertexLoweringInfo vertexStageLowering = {};
	bool bootstrapVertexPushConstantOffsetValid = false;
	float bootstrapPointSizeValue = 64.0f;
	bool bootstrapFragmentConfigValid = false;
	FragmentBootstrapConfig bootstrapFragmentConfigValue = {};
	bool texturePlanValid = false;
	GraphicsExecutableTexturePlan texturePlanValue = {};
	bool imageResourcePlanValid = false;
	GraphicsExecutableImageResourcePlan imageResourcePlanValue = {};
	bool resourcePlanValid = false;
	GraphicsExecutableResourcePlan resourcePlanValue = {};
	uint32_t fragmentFeatureMaskValue = 0;
	uint32_t triangleBootstrapUnsupportedReasonMaskValue = 0;
};

}  // namespace backend

#endif  // SWIFTSHADER_GRAPHICS_EXECUTABLE_HPP_
