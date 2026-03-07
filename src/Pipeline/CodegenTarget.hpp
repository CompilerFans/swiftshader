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

	bool operator==(const NormalizedAbiDescription &rhs) const
	{
		return header.descriptorSetCount == rhs.header.descriptorSetCount &&
		       header.dynamicOffsetCount == rhs.header.dynamicOffsetCount &&
		       header.pushConstantSize == rhs.header.pushConstantSize &&
		       header.reserved == rhs.header.reserved &&
		       fragment.quadWidth == rhs.fragment.quadWidth &&
		       fragment.quadHeight == rhs.fragment.quadHeight &&
		       fragment.helperLaneMask == rhs.fragment.helperLaneMask &&
		       fragment.exportMask == rhs.fragment.exportMask;
	}
};

}  // namespace sw

#endif  // SWIFTSHADER_CODEGEN_TARGET_HPP_
