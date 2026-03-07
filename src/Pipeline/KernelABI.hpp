#ifndef SWIFTSHADER_KERNEL_ABI_HPP_
#define SWIFTSHADER_KERNEL_ABI_HPP_

#include <cstdint>

namespace sw {

struct KernelABIHeader
{
	uint32_t descriptorSetCount;
	uint32_t dynamicOffsetCount;
	uint32_t pushConstantSize;
	uint32_t reserved;
};

}  // namespace sw

#endif  // SWIFTSHADER_KERNEL_ABI_HPP_
