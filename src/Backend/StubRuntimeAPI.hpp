#ifndef SWIFTSHADER_STUB_RUNTIME_API_HPP_
#define SWIFTSHADER_STUB_RUNTIME_API_HPP_

#include "RuntimeAPI.hpp"

#include <unordered_map>
#include <vector>

namespace backend {

class StubRuntimeAPI : public RuntimeAPI
{
public:
	static void resetGlobalCapture();
	static const std::string &globalLastModuleSource();
	static const LaunchRecord &globalLastLaunch();
	static uint32_t globalLaunchCount();

	bool isHardwareBacked() const override { return false; }
	ModuleHandle createModule(const std::string &sourceOrIR, const std::string &entryPoint = "kernel_main") override;
	DeviceMemoryHandle allocateMemory(size_t numBytes) override;
	void freeMemory(DeviceMemoryHandle memory) override;
	void copyHostToMemory(DeviceMemoryHandle memory, const void *source, size_t numBytes) override;
	void copyMemoryToHost(void *destination, DeviceMemoryHandle memory, size_t numBytes) override;
	uint64_t memoryAddress(DeviceMemoryHandle memory) const override;
	void launch(ModuleHandle module, const LaunchRecord &record, const std::vector<void *> &arguments) override;
	void synchronize() override {}

	const std::string &lastModuleSource() const { return moduleSource; }
	const LaunchRecord &lastLaunch() const { return launchRecord; }

private:
	uint64_t nextId = 1;
	uint64_t nextMemoryId = 1;
	std::string moduleSource;
	LaunchRecord launchRecord = {};
	std::unordered_map<uint64_t, std::string> moduleEntrypoints;
	std::unordered_map<uint64_t, std::vector<uint8_t>> allocations;
};

}  // namespace backend

#endif  // SWIFTSHADER_STUB_RUNTIME_API_HPP_
