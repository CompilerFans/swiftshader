#include "ShaderCompiler.hpp"

#include "CudaLikeSourceEmitter.hpp"
#include "KernelIRLowering.hpp"
#include "LlvmIREmitter.hpp"

namespace sw {

ShaderCompilerOutput ShaderCompiler::compileGraphicsFragment(const ShaderModuleInput &input,
                                                            const ShaderCompilerAnalysisContext &context,
                                                            CodegenTarget target) const
{
	ShaderCompilerOutput output = {};
	output.analysis = analyzeGraphicsFragmentShader(input, context);
	applyCompilerAnalysisToKernelIR(output.analysis, &output.kernelIR);

	switch(target)
	{
	case CodegenTarget::CudaLikeSource:
		output.text = emitCudaLikeSource(output.kernelIR);
		break;
	case CodegenTarget::LlvmIR:
		output.text = emitLlvmIR(output.kernelIR);
		break;
	}

	return output;
}

}  // namespace sw
