#ifndef SWIFTSHADER_CUDA_RUNTIME_API_HPP_
#define SWIFTSHADER_CUDA_RUNTIME_API_HPP_

#include "RuntimeAPI.hpp"

#include <memory>
#include <string>

namespace backend {

class CudaRuntimeAPI : public RuntimeAPI
{
public:
	CudaRuntimeAPI();
	~CudaRuntimeAPI() override;

	static void resetGlobalCapture();
	static const std::string &globalLastModuleSource();
	static const LaunchRecord &globalLastLaunch();
	static uint32_t globalLaunchCount();
	static uint32_t globalModuleCompilationCount();
	static uint32_t globalModuleCacheHitCount();

	bool isAvailable() const;
	const std::string &initializationError() const;

	bool isHardwareBacked() const override;
	ModuleHandle createModule(const std::string &sourceOrIR, const std::string &entryPoint = "kernel_main") override;
	DeviceMemoryHandle allocateMemory(size_t numBytes) override;
	void freeMemory(DeviceMemoryHandle memory) override;
	void copyHostToMemory(DeviceMemoryHandle memory, const void *source, size_t numBytes) override;
	void copyMemoryToHost(void *destination, DeviceMemoryHandle memory, size_t numBytes) override;
	uint64_t memoryAddress(DeviceMemoryHandle memory) const override;
	void launch(ModuleHandle module, const LaunchRecord &record, const std::vector<void *> &arguments) override;
	void synchronize() override;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}  // namespace backend

#endif  // SWIFTSHADER_CUDA_RUNTIME_API_HPP_
