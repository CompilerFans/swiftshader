#ifndef SWIFTSHADER_KERNEL_IR_LOWERING_HPP_
#define SWIFTSHADER_KERNEL_IR_LOWERING_HPP_

#include "KernelIR.hpp"
#include "SemanticIR.hpp"

namespace sw {

inline KernelIRModule lowerToKernelIR(const SemanticIRModule &semantic)
{
	KernelIRModule kernel;
	if(semantic.stage() == VK_SHADER_STAGE_VERTEX_BIT)
	{
		kernel.setVertexLoweringInfo(semantic.vertexLowering());
	}
	return kernel;
}

}  // namespace sw

#endif  // SWIFTSHADER_KERNEL_IR_LOWERING_HPP_
