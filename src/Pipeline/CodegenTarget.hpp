#ifndef SWIFTSHADER_CODEGEN_TARGET_HPP_
#define SWIFTSHADER_CODEGEN_TARGET_HPP_

#include "KernelABI.hpp"
#include "KernelIR.hpp"

namespace sw {

enum class CodegenTarget
{
	CudaLikeSource,
	LlvmIR,
};

struct NormalizedAbiDescription
{
	KernelABIHeader header{};
	FragmentExecutionInfo fragment{};
	CompilerAnalysisInfo compilerAnalysis{};

	bool operator==(const NormalizedAbiDescription &rhs) const
	{
		return header.descriptorSetCount == rhs.header.descriptorSetCount &&
		       header.dynamicOffsetCount == rhs.header.dynamicOffsetCount &&
		       header.pushConstantSize == rhs.header.pushConstantSize &&
		       header.reserved == rhs.header.reserved &&
		       fragment.quadWidth == rhs.fragment.quadWidth &&
		       fragment.quadHeight == rhs.fragment.quadHeight &&
		       fragment.helperLaneMask == rhs.fragment.helperLaneMask &&
		       fragment.exportMask == rhs.fragment.exportMask &&
		       compilerAnalysis.fragmentFeatureMask == rhs.compilerAnalysis.fragmentFeatureMask &&
		       compilerAnalysis.unsupportedReasonMask == rhs.compilerAnalysis.unsupportedReasonMask &&
		       compilerAnalysis.hasTexturePlan == rhs.compilerAnalysis.hasTexturePlan &&
		       compilerAnalysis.textureResourceKind == rhs.compilerAnalysis.textureResourceKind &&
		       compilerAnalysis.textureBootstrapSupported == rhs.compilerAnalysis.textureBootstrapSupported &&
		       compilerAnalysis.hasImageResourcePlan == rhs.compilerAnalysis.hasImageResourcePlan &&
		       compilerAnalysis.hasResourcePlan == rhs.compilerAnalysis.hasResourcePlan;
	}
};

}  // namespace sw

#endif  // SWIFTSHADER_CODEGEN_TARGET_HPP_
