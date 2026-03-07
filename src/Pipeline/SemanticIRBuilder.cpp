#include "SemanticIRBuilder.hpp"

#include "SpirvShader.hpp"

namespace sw {

std::shared_ptr<SemanticIRModule> SemanticIRBuilder::build(const SpirvShader &shader) const
{
	ParsedSpirvInfo parsed = { shader.getStage(), shader.getEntryPointName() };
	return build(parsed);
}

}  // namespace sw
