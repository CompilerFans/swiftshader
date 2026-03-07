#ifndef SWIFTSHADER_LLVM_IR_EMITTER_HPP_
#define SWIFTSHADER_LLVM_IR_EMITTER_HPP_

#include "CodegenTarget.hpp"

#include <string>

namespace sw {

std::string emitLlvmIR(const KernelIRModule &module);
NormalizedAbiDescription describeLlvmAbi(const KernelIRModule &module);

}  // namespace sw

#endif  // SWIFTSHADER_LLVM_IR_EMITTER_HPP_
