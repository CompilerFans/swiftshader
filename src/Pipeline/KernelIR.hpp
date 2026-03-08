#ifndef SWIFTSHADER_KERNEL_IR_HPP_
#define SWIFTSHADER_KERNEL_IR_HPP_

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

private:
	FragmentExecutionInfo fragment = {};
	VertexLoweringInfo vertex = {};
	bool hasVertexLowering = false;
};

}  // namespace sw

#endif  // SWIFTSHADER_KERNEL_IR_HPP_
