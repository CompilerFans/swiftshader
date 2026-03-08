#include "SemanticIRBuilder.hpp"

#include "SpirvShader.hpp"

namespace sw {

std::shared_ptr<SemanticIRModule> SemanticIRBuilder::build(const SpirvShader &shader) const
{
	return build(shader.getStage(), shader.getEntryPointName(), shader.insns);
}

}  // namespace sw
