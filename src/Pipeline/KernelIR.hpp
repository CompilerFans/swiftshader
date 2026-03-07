#ifndef SWIFTSHADER_KERNEL_IR_HPP_
#define SWIFTSHADER_KERNEL_IR_HPP_

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

private:
	FragmentExecutionInfo fragment = {};
};

}  // namespace sw

#endif  // SWIFTSHADER_KERNEL_IR_HPP_
