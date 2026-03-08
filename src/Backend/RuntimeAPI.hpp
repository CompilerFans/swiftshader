#ifndef SWIFTSHADER_RUNTIME_API_HPP_
#define SWIFTSHADER_RUNTIME_API_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace backend {

struct ModuleHandle
{
	uint64_t id = 0;

	bool valid() const
	{
		return id != 0;
	}
};

struct DeviceMemoryHandle
{
	uint64_t id = 0;

	bool valid() const
	{
		return id != 0;
	}
};

struct LaunchRecord
{
	ModuleHandle module = {};
	uint32_t baseGroupX = 0;
	uint32_t baseGroupY = 0;
	uint32_t baseGroupZ = 0;
	uint32_t groupCountX = 0;
	uint32_t groupCountY = 0;
	uint32_t groupCountZ = 0;
	uint32_t blockCountX = 1;
	uint32_t blockCountY = 1;
	uint32_t blockCountZ = 1;
	size_t bindingCount = 0;
	size_t argumentWords = 0;
	size_t argumentCount = 0;
};

class RuntimeAPI
{
public:
	virtual ~RuntimeAPI() = default;

	virtual bool isHardwareBacked() const = 0;
	virtual ModuleHandle createModule(const std::string &sourceOrIR) = 0;
	virtual DeviceMemoryHandle allocateMemory(size_t numBytes) = 0;
	virtual void freeMemory(DeviceMemoryHandle memory) = 0;
	virtual void copyHostToMemory(DeviceMemoryHandle memory, const void *source, size_t numBytes) = 0;
	virtual void copyMemoryToHost(void *destination, DeviceMemoryHandle memory, size_t numBytes) = 0;
	virtual uint64_t memoryAddress(DeviceMemoryHandle memory) const = 0;
	virtual void launch(ModuleHandle module, const LaunchRecord &record, const std::vector<void *> &arguments) = 0;
	virtual void synchronize() = 0;
};

}  // namespace backend

#endif  // SWIFTSHADER_RUNTIME_API_HPP_
