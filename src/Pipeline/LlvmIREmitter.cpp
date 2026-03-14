#include "LlvmIREmitter.hpp"

#include <sstream>

namespace sw {

std::string emitLlvmIR(const KernelIRModule &module)
{
	std::ostringstream ir;
	const auto &analysis = module.compilerAnalysisInfo();
	ir << "@swiftshader.fragment_feature_mask = internal constant i32 " << analysis.fragmentFeatureMask << "\n";
	ir << "@swiftshader.unsupported_reason_mask = internal constant i32 " << analysis.unsupportedReasonMask << "\n";
	ir << "@swiftshader.has_texture_plan = internal constant i1 " << (analysis.hasTexturePlan ? "true" : "false") << "\n";
	ir << "@swiftshader.has_image_resource_plan = internal constant i1 " << (analysis.hasImageResourcePlan ? "true" : "false") << "\n";
	ir << "@swiftshader.has_resource_plan = internal constant i1 " << (analysis.hasResourcePlan ? "true" : "false") << "\n";
	ir << "define void @kernel_main() {\n  ret void\n}\n";
	return ir.str();
}

NormalizedAbiDescription describeLlvmAbi(const KernelIRModule &module)
{
	NormalizedAbiDescription abi = {};
	abi.fragment = module.fragmentExecutionInfo();
	abi.compilerAnalysis = module.compilerAnalysisInfo();
	return abi;
}

}  // namespace sw
