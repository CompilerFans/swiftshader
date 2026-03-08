#ifndef SWIFTSHADER_SEMANTIC_IR_BUILDER_HPP_
#define SWIFTSHADER_SEMANTIC_IR_BUILDER_HPP_

#include "SpirvBinary.hpp"
#include "SemanticIR.hpp"

#include <memory>
#include <string>

namespace sw {

class SpirvShader;

struct ParsedSpirvInfo
{
	VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	std::string entryPointName;
	VertexLoweringInfo vertexLowering;
};

class SemanticIRBuilder
{
public:
	std::shared_ptr<SemanticIRModule> build(const ParsedSpirvInfo &parsed) const
	{
		return std::make_shared<SemanticIRModule>(parsed.stage, parsed.entryPointName, parsed.vertexLowering);
	}

	std::shared_ptr<SemanticIRModule> build(VkShaderStageFlagBits stage, const std::string &entryPointName, const SpirvBinary &spirv) const;
	std::shared_ptr<SemanticIRModule> build(const SpirvShader &shader) const;
};

}  // namespace sw

#endif  // SWIFTSHADER_SEMANTIC_IR_BUILDER_HPP_
