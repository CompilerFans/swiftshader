#ifndef SWIFTSHADER_KERNEL_IR_LOWERING_HPP_
#define SWIFTSHADER_KERNEL_IR_LOWERING_HPP_

#include "KernelIR.hpp"
#include "SemanticIR.hpp"
#include "ShaderCompilerAnalysis.hpp"

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

inline void applyCompilerAnalysisToKernelIR(const ShaderCompilerAnalysisResult &analysis,
                                            KernelIRModule *kernel)
{
	if(kernel == nullptr)
	{
		return;
	}

	CompilerAnalysisInfo info = {};
	info.fragmentFeatureMask = analysis.fragmentFeatureMask;
	info.unsupportedReasonMask = analysis.unsupportedReasonMask;
	info.hasTexturePlan = (analysis.texturePlan.resourceKind != ShaderTextureResourceKind::None);
	info.textureResourceKind = analysis.texturePlan.resourceKind;
	info.textureBootstrapSupported = analysis.texturePlan.bootstrapSupported;
	info.hasImageResourcePlan = !analysis.imageResourcePlan.sampledDescriptors.empty() ||
	                            !analysis.imageResourcePlan.storageDescriptors.empty();
	info.hasResourcePlan = analysis.resourcePlan.descriptorSetCount != 0 ||
	                       analysis.resourcePlan.dynamicOffsetCount != 0 ||
	                       analysis.resourcePlan.pushConstantSize != 0 ||
	                       !analysis.resourcePlan.descriptors.empty();
	info.staticFragmentKind = analysis.staticFragmentKind;
	info.colorR = analysis.colorR;
	info.colorG = analysis.colorG;
	info.colorB = analysis.colorB;
	info.colorA = analysis.colorA;
	kernel->setCompilerAnalysisInfo(info);
}

}  // namespace sw

#endif  // SWIFTSHADER_KERNEL_IR_LOWERING_HPP_
