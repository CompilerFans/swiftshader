#ifndef SWIFTSHADER_SEMANTIC_IR_HPP_
#define SWIFTSHADER_SEMANTIC_IR_HPP_

#include "VertexLoweringInfo.hpp"
#include "Vulkan/VulkanPlatform.hpp"

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
	SemanticIRModule(VkShaderStageFlagBits stage, std::string entryPoint, VertexLoweringInfo vertexLowering = {})
	    : shaderStage(stage)
	    , mainEntryPoint(std::move(entryPoint))
	    , vertex(std::move(vertexLowering))
	{}

	VkShaderStageFlagBits stage() const
	{
		return shaderStage;
	}

	const std::string &entryPoint() const
	{
		return mainEntryPoint;
	}

	const VertexLoweringInfo &vertexLowering() const
	{
		return vertex;
	}

private:
	VkShaderStageFlagBits shaderStage;
	std::string mainEntryPoint;
	VertexLoweringInfo vertex;
};

}  // namespace sw

#endif  // SWIFTSHADER_SEMANTIC_IR_HPP_
