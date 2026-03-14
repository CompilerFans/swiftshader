#include "LlvmIREmitter.hpp"

#include <iomanip>
#include <sstream>

namespace sw {

namespace {

std::string emitLlvmFloatLiteral(float value)
{
	std::ostringstream stream;
	stream << std::scientific << std::setprecision(6) << value;
	return stream.str();
}

void emitCompilerAnalysisGlobals(std::ostringstream &ir, const CompilerAnalysisInfo &analysis)
{
	ir << "@swiftshader.fragment_feature_mask = internal constant i32 " << analysis.fragmentFeatureMask << "\n";
	ir << "@swiftshader.unsupported_reason_mask = internal constant i32 " << analysis.unsupportedReasonMask << "\n";
	ir << "@swiftshader.has_texture_plan = internal constant i1 " << (analysis.hasTexturePlan ? "true" : "false") << "\n";
	ir << "@swiftshader.has_image_resource_plan = internal constant i1 " << (analysis.hasImageResourcePlan ? "true" : "false") << "\n";
	ir << "@swiftshader.has_resource_plan = internal constant i1 " << (analysis.hasResourcePlan ? "true" : "false") << "\n";
}

std::string emitConstantColorFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "@swiftshader.color_r = internal constant float " << emitLlvmFloatLiteral(analysis.colorR) << "\n";
	ir << "@swiftshader.color_g = internal constant float " << emitLlvmFloatLiteral(analysis.colorG) << "\n";
	ir << "@swiftshader.color_b = internal constant float " << emitLlvmFloatLiteral(analysis.colorB) << "\n";
	ir << "@swiftshader.color_a = internal constant float " << emitLlvmFloatLiteral(analysis.colorA) << "\n";
	ir << "%struct.FragmentInvocation = type { i32, i32 }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32 }\n";
	ir << "define internal i8 @packColor(float %value) {\n";
	ir << "entry:\n";
	ir << "  ret i8 0\n";
	ir << "}\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitTextureFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "%struct.FragmentInvocation = type { i32, i32 }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32, i8*, i32, i32, i32, i32, i32, i32 }\n";
	ir << "define internal void @sampleTexture(%struct.FsParams* %params, float %u, float %v, float* %r, float* %g, float* %b, float* %a) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitFragCoordQuadrantsFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "%struct.FragmentInvocation = type { i32, i32, i32 }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32 }\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitFrontFacingFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "%struct.FragmentInvocation = type { i32, i32, i32 }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32 }\n";
	ir << "@swiftshader.frontFacing_label = internal constant [12 x i8] c\"frontFacing\\00\"\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitFragCoordDiscardLeftConstantColorFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "@swiftshader.color_r = internal constant float 1.000000e+00\n";
	ir << "%struct.FragmentInvocation = type { i32, i32, i32 }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32 }\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitPointCoordGradientFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "@swiftshader.pointCoordX = internal constant float 0.000000e+00\n";
	ir << "@swiftshader.pointCoordY = internal constant float 0.000000e+00\n";
	ir << "%struct.FragmentInvocation = type { i32, i32, i32, float, float }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32 }\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitFlatInterpolatedColorFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "@swiftshader.vertexColor0R = internal constant float 0.000000e+00\n";
	ir << "@swiftshader.vertexColor0G = internal constant float 0.000000e+00\n";
	ir << "@swiftshader.vertexColor0B = internal constant float 0.000000e+00\n";
	ir << "%struct.FragmentInvocation = type { i32, i32 }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32, float, float, float, float }\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitInterpolatedColorFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "@swiftshader.vertexColor2B = internal constant float 0.000000e+00\n";
	ir << "%struct.FragmentInvocation = type { i32, i32, float, float, float }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32, float, float, float, float, float, float, float, float, float, float, float, float }\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  %barycentric0 = alloca float\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

std::string emitInterpolatedColorFragDepthFragmentLlvmIR(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
	ir << "@swiftshader.nearDepth = internal constant float 2.000000e-01\n";
	ir << "@swiftshader.farDepth = internal constant float 8.000000e-01\n";
	ir << "%struct.FragmentInvocation = type { i32, i32, float, float, float }\n";
	ir << "%struct.FsParams = type { %struct.FragmentInvocation*, i8*, i32, i32, i32, float*, float, float, float, float, float, float, float, float, float, float, float, float, float }\n";
	ir << "define void @fs_entry(%struct.FsParams* %params) {\n";
	ir << "entry:\n";
	ir << "  ; outDepth = colorB > colorR ? params.nearDepth : params.farDepth;\n";
	ir << "  ret void\n";
	ir << "}\n";
	return ir.str();
}

}  // namespace

std::string emitLlvmIR(const KernelIRModule &module)
{
	const auto &analysis = module.compilerAnalysisInfo();
	if(analysis.hasTexturePlan &&
	   analysis.textureBootstrapSupported &&
	   (analysis.textureResourceKind == ShaderTextureResourceKind::CombinedImageSampler ||
	    analysis.textureResourceKind == ShaderTextureResourceKind::SeparateImageSampler))
	{
		return emitTextureFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::ConstantColor)
	{
		return emitConstantColorFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::FragCoordQuadrants)
	{
		return emitFragCoordQuadrantsFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::FrontFacingBinaryColors)
	{
		return emitFrontFacingFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::FragCoordDiscardLeftConstantColor)
	{
		return emitFragCoordDiscardLeftConstantColorFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::PointCoordGradient)
	{
		return emitPointCoordGradientFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::FlatInterpolatedColor)
	{
		return emitFlatInterpolatedColorFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::InterpolatedColor)
	{
		return emitInterpolatedColorFragmentLlvmIR(analysis);
	}
	if(analysis.staticFragmentKind == ShaderStaticFragmentKind::InterpolatedColorBlueNearFragDepth)
	{
		return emitInterpolatedColorFragDepthFragmentLlvmIR(analysis);
	}

	std::ostringstream ir;
	emitCompilerAnalysisGlobals(ir, analysis);
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
