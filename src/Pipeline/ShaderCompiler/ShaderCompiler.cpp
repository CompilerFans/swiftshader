#include "ShaderCompiler.hpp"

#include "../CudaLikeSourceEmitter.hpp"
#include "../KernelIRLowering.hpp"
#include "../LlvmIREmitter.hpp"
#include "../SemanticIRBuilder.hpp"

#include "spirv-tools/libspirv.hpp"

#include <vector>

namespace sw {

namespace {

bool materializeSpirvBinary(const ShaderModuleInput &input, SpirvBinary *spirv)
{
	if(spirv == nullptr)
	{
		return false;
	}

	switch(input.kind())
	{
	case ShaderModuleInput::Kind::SpirvBinary:
		*spirv = input.spirvBinary();
		return true;
	case ShaderModuleInput::Kind::SpirvAssemblyText:
		break;
	}

	spvtools::SpirvTools core(SPV_ENV_VULKAN_1_0);
	std::vector<uint32_t> words;
	if(!core.Assemble(input.spirvAssembly(), &words))
	{
		return false;
	}
	if(!core.Validate(words))
	{
		return false;
	}

	*spirv = SpirvBinary(words.data(), static_cast<uint32_t>(words.size()));
	return true;
}

}  // namespace

ShaderCompilerOutput ShaderCompiler::compileGraphicsVertex(const ShaderModuleInput &input,
                                                           CodegenTarget target) const
{
	ShaderCompilerOutput output = {};

	SpirvBinary spirv;
	if(!materializeSpirvBinary(input, &spirv))
	{
		return output;
	}

	SemanticIRBuilder builder;
	auto semantic = builder.build(VK_SHADER_STAGE_VERTEX_BIT, input.entryPoint(), spirv);
	if(!semantic)
	{
		return output;
	}

	output.kernelIR = lowerToKernelIR(*semantic);

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
