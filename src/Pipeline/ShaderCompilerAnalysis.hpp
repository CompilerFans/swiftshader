#ifndef SWIFTSHADER_SHADER_COMPILER_ANALYSIS_HPP_
#define SWIFTSHADER_SHADER_COMPILER_ANALYSIS_HPP_

#include "ShaderModuleInput.hpp"
#include "SpirvBinary.hpp"
#include "Vulkan/VkConfig.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sw {

enum class ShaderTextureResourceKind : uint32_t
{
	None = 0,
	CombinedImageSampler = 1,
	SeparateImageSampler = 2,
	Other = 3,
};

enum class ShaderFragmentFeature : uint32_t
{
	None = 0,
	Discard = 1u << 0,
	StorageImageReadWrite = 1u << 1,
	ImageQueryOrFetch = 1u << 2,
	Derivatives = 1u << 3,
	Atomics = 1u << 4,
	Subgroup = 1u << 5,
};

enum class ShaderUnsupportedReason : uint32_t
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

struct ShaderDescriptorRef
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

struct ShaderTexturePlan
{
	ShaderTextureResourceKind resourceKind = ShaderTextureResourceKind::None;
	uint32_t imageDescriptorSet = 0;
	uint32_t imageBinding = 0;
	uint32_t imageArrayElement = 0;
	uint32_t samplerDescriptorSet = 0;
	uint32_t samplerBinding = 0;
	uint32_t samplerArrayElement = 0;
	bool bootstrapSupported = false;
};

struct ShaderImageResourcePlan
{
	std::vector<ShaderDescriptorRef> sampledDescriptors;
	std::vector<ShaderDescriptorRef> storageDescriptors;
};

struct ShaderResourcePlan
{
	uint32_t descriptorSetCount = 0;
	uint32_t dynamicOffsetCount = 0;
	uint32_t pushConstantSize = 0;
	std::vector<ShaderDescriptorRef> descriptors;
};

struct ShaderDescriptorBindingInfo
{
	VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	uint32_t descriptorCount = 0;
	bool isDynamic = false;
	uint32_t dynamicOffsetIndex = 0;
};

using ShaderCompilerAnalysisQueryDescriptorBindingInfo = bool (*)(const void *userdata,
                                                                  uint32_t descriptorSet,
                                                                  uint32_t binding,
                                                                  ShaderDescriptorBindingInfo *bindingInfo);

struct ShaderCompilerAnalysisContext
{
	ShaderCompilerAnalysisQueryDescriptorBindingInfo queryDescriptorBindingInfo = nullptr;
	const void *queryDescriptorBindingInfoUserdata = nullptr;
	uint32_t descriptorSetCount = 0;
	uint32_t dynamicOffsetCount = 0;
	uint32_t pushConstantSize = vk::MAX_PUSH_CONSTANT_SIZE;
};

struct ShaderCompilerAnalysisResult
{
	ShaderTexturePlan texturePlan = {};
	ShaderImageResourcePlan imageResourcePlan = {};
	ShaderResourcePlan resourcePlan = {};
	uint32_t fragmentFeatureMask = 0;
	uint32_t unsupportedReasonMask = 0;
};

ShaderCompilerAnalysisResult analyzeGraphicsFragmentShader(const std::string &entryPointName,
                                                          const SpirvBinary &spirv,
                                                          const ShaderCompilerAnalysisContext &context);
ShaderCompilerAnalysisResult analyzeGraphicsFragmentShader(const ShaderModuleInput &input,
                                                          const ShaderCompilerAnalysisContext &context);
ShaderCompilerAnalysisResult analyzeGraphicsFragmentShaderAssembly(const std::string &entryPointName,
                                                                  const std::string &spirvAssembly,
                                                                  const ShaderCompilerAnalysisContext &context);

}  // namespace sw

#endif  // SWIFTSHADER_SHADER_COMPILER_ANALYSIS_HPP_
