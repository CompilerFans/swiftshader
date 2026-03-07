#include "LlvmIREmitter.hpp"

namespace sw {

std::string emitLlvmIR(const KernelIRModule &module)
{
	(void)module;
	return "define void @kernel_main() {\n  ret void\n}\n";
}

NormalizedAbiDescription describeLlvmAbi(const KernelIRModule &module)
{
	NormalizedAbiDescription abi = {};
	abi.fragment = module.fragmentExecutionInfo();
	return abi;
}

}  // namespace sw
