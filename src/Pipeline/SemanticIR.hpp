#ifndef SWIFTSHADER_SEMANTIC_IR_HPP_
#define SWIFTSHADER_SEMANTIC_IR_HPP_

#include <vulkan/vulkan_core.h>

#include <string>

namespace sw {

enum class ResourceAccessKind
{
	CombinedImageSampler,
	SeparateImage,
	SeparateSampler,
	StorageImage,
	TexelBuffer,
};

class SemanticIRModule
{
public:
	SemanticIRModule(VkShaderStageFlagBits stage, std::string entryPoint)
	    : shaderStage(stage)
	    , mainEntryPoint(std::move(entryPoint))
	{}

	VkShaderStageFlagBits stage() const
	{
		return shaderStage;
	}

	const std::string &entryPoint() const
	{
		return mainEntryPoint;
	}

private:
	VkShaderStageFlagBits shaderStage;
	std::string mainEntryPoint;
};

}  // namespace sw

#endif  // SWIFTSHADER_SEMANTIC_IR_HPP_
