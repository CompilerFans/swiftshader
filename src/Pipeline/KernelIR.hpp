#ifndef SWIFTSHADER_KERNEL_IR_HPP_
#define SWIFTSHADER_KERNEL_IR_HPP_

#include "ShaderCompilerAnalysis.hpp"
#include "VertexLoweringInfo.hpp"

#include <cstdint>

namespace sw {

struct FragmentExecutionInfo
{
	uint32_t quadWidth = 2;
	uint32_t quadHeight = 2;
	uint32_t helperLaneMask = 0;
	uint32_t exportMask = 0;
};

struct CompilerAnalysisInfo
{
	uint32_t fragmentFeatureMask = 0;
	uint32_t unsupportedReasonMask = 0;
	bool hasTexturePlan = false;
	ShaderTextureResourceKind textureResourceKind = ShaderTextureResourceKind::None;
	bool textureBootstrapSupported = false;
	bool hasImageResourcePlan = false;
	bool hasResourcePlan = false;
	ShaderStaticFragmentKind staticFragmentKind = ShaderStaticFragmentKind::None;
	float colorR = 0.0f;
	float colorG = 0.0f;
	float colorB = 0.0f;
	float colorA = 0.0f;
};

class KernelIRModule
{
public:
	KernelIRModule() = default;

	void setFragmentExecutionInfo(const FragmentExecutionInfo &value)
	{
		fragment = value;
	}

	const FragmentExecutionInfo &fragmentExecutionInfo() const
	{
		return fragment;
	}

	void setVertexLoweringInfo(const VertexLoweringInfo &value)
	{
		vertex = value;
		hasVertexLowering = true;
	}

	bool hasVertexLoweringInfo() const
	{
		return hasVertexLowering;
	}

	const VertexLoweringInfo &vertexLoweringInfo() const
	{
		return vertex;
	}

	void setCompilerAnalysisInfo(const CompilerAnalysisInfo &value)
	{
		compilerAnalysis = value;
	}

	const CompilerAnalysisInfo &compilerAnalysisInfo() const
	{
		return compilerAnalysis;
	}

private:
	FragmentExecutionInfo fragment = {};
	VertexLoweringInfo vertex = {};
	bool hasVertexLowering = false;
	CompilerAnalysisInfo compilerAnalysis = {};
};

}  // namespace sw

#endif  // SWIFTSHADER_KERNEL_IR_HPP_
