#include "CudaLikeSourceEmitter.hpp"

namespace sw {

std::string emitCudaLikeSource(const KernelIRModule &module)
{
	(void)module;
	return "extern \"C\" __global__ void kernel_main() {}\n";
}

NormalizedAbiDescription describeCudaLikeAbi(const KernelIRModule &module)
{
	NormalizedAbiDescription abi = {};
	abi.fragment = module.fragmentExecutionInfo();
	return abi;
}

}  // namespace sw
