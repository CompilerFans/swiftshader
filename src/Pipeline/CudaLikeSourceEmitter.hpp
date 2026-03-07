#ifndef SWIFTSHADER_CUDA_LIKE_SOURCE_EMITTER_HPP_
#define SWIFTSHADER_CUDA_LIKE_SOURCE_EMITTER_HPP_

#include "CodegenTarget.hpp"

#include <string>

namespace sw {

std::string emitCudaLikeSource(const KernelIRModule &module);
NormalizedAbiDescription describeCudaLikeAbi(const KernelIRModule &module);

}  // namespace sw

#endif  // SWIFTSHADER_CUDA_LIKE_SOURCE_EMITTER_HPP_
