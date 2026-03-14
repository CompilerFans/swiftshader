#ifndef SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_COMPILER_HPP_
#define SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_COMPILER_HPP_

#include "CodegenTarget.hpp"
#include "KernelIR.hpp"
#include "ShaderCompilerAnalysis.hpp"
#include "ShaderModuleInput.hpp"

#include <string>

namespace sw {

struct ShaderCompilerOutput
{
	ShaderCompilerAnalysisResult analysis = {};
	KernelIRModule kernelIR = {};
	std::string text;
};

class ShaderCompiler
{
public:
	ShaderCompilerOutput compileGraphicsFragment(const ShaderModuleInput &input,
	                                            const ShaderCompilerAnalysisContext &context,
	                                            CodegenTarget target) const;
};

}  // namespace sw

#endif  // SWIFTSHADER_PIPELINE_SHADER_COMPILER_SHADER_COMPILER_HPP_
