#include "CudaRuntimeAPI.hpp"

#include "CudaCompilerDriver.hpp"
#include "System/SharedLibrary.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <cuda.h>

namespace backend {
namespace {

std::string gLastModuleSource;
LaunchRecord gLastLaunch = {};
uint32_t gLaunchCount = 0;

const char *kCudaLibraries[] = {
	"libcuda.so.1",
	"libcuda.so",
};

const char *kKernelName = "kernel_main";

void recordLaunchStamp()
{
	const char *stampPath = std::getenv("SWIFTSHADER_CUDA_LAUNCH_STAMP");
	if(!stampPath || stampPath[0] == '\0')
	{
		return;
	}

	std::ofstream stream(stampPath, std::ios::app);
	if(stream.is_open())
	{
		stream << "1\n";
	}
}

bool shouldDumpCudaSource()
{
	const char *value = std::getenv("SWIFTSHADER_CUDA_DUMP_SOURCE");
	if(!value || value[0] == '\0')
	{
		return true;
	}

	std::string normalized(value);
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});

	return normalized != "0" && normalized != "false" && normalized != "off" && normalized != "no";
}

void dumpCudaSource(const std::string &source)
{
	if(!shouldDumpCudaSource())
	{
		return;
	}

	std::fputs("=== SWIFTSHADER CUDA SOURCE BEGIN ===\n", stderr);
	std::fwrite(source.data(), 1, source.size(), stderr);
	if(source.empty() || source.back() != '\n')
	{
		std::fputc('\n', stderr);
	}
	std::fputs("=== SWIFTSHADER CUDA SOURCE END ===\n", stderr);
	std::fflush(stderr);
}

}  // namespace

struct CudaRuntimeAPI::Impl
{
	using CuInitFn = CUresult(CUDAAPI *)(unsigned int);
	using CuDeviceGetCountFn = CUresult(CUDAAPI *)(int *);
	using CuDeviceGetFn = CUresult(CUDAAPI *)(CUdevice *, int);
	using CuDeviceGetAttributeFn = CUresult(CUDAAPI *)(int *, CUdevice_attribute, CUdevice);
	using CuCtxCreateFn = CUresult(CUDAAPI *)(CUcontext *, unsigned int, CUdevice);
	using CuCtxDestroyFn = CUresult(CUDAAPI *)(CUcontext);
	using CuCtxSetCurrentFn = CUresult(CUDAAPI *)(CUcontext);
	using CuCtxSynchronizeFn = CUresult(CUDAAPI *)(void);
	using CuModuleLoadFn = CUresult(CUDAAPI *)(CUmodule *, const char *);
	using CuModuleUnloadFn = CUresult(CUDAAPI *)(CUmodule);
	using CuModuleGetFunctionFn = CUresult(CUDAAPI *)(CUfunction *, CUmodule, const char *);
	using CuLaunchKernelFn = CUresult(CUDAAPI *)(CUfunction, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, CUstream, void **, void **);
	using CuMemAllocFn = CUresult(CUDAAPI *)(CUdeviceptr *, size_t);
	using CuMemFreeFn = CUresult(CUDAAPI *)(CUdeviceptr);
	using CuMemcpyHtoDFn = CUresult(CUDAAPI *)(CUdeviceptr, const void *, size_t);
	using CuMemcpyDtoHFn = CUresult(CUDAAPI *)(void *, CUdeviceptr, size_t);
	using CuGetErrorNameFn = CUresult(CUDAAPI *)(CUresult, const char **);
	using CuGetErrorStringFn = CUresult(CUDAAPI *)(CUresult, const char **);

	struct DriverTable
	{
		CuInitFn cuInit = nullptr;
		CuDeviceGetCountFn cuDeviceGetCount = nullptr;
		CuDeviceGetFn cuDeviceGet = nullptr;
		CuDeviceGetAttributeFn cuDeviceGetAttribute = nullptr;
		CuCtxCreateFn cuCtxCreate = nullptr;
		CuCtxDestroyFn cuCtxDestroy = nullptr;
		CuCtxSetCurrentFn cuCtxSetCurrent = nullptr;
		CuCtxSynchronizeFn cuCtxSynchronize = nullptr;
		CuModuleLoadFn cuModuleLoad = nullptr;
		CuModuleUnloadFn cuModuleUnload = nullptr;
		CuModuleGetFunctionFn cuModuleGetFunction = nullptr;
		CuLaunchKernelFn cuLaunchKernel = nullptr;
		CuMemAllocFn cuMemAlloc = nullptr;
		CuMemFreeFn cuMemFree = nullptr;
		CuMemcpyHtoDFn cuMemcpyHtoD = nullptr;
		CuMemcpyDtoHFn cuMemcpyDtoH = nullptr;
		CuGetErrorNameFn cuGetErrorName = nullptr;
		CuGetErrorStringFn cuGetErrorString = nullptr;
	};

	void *cudaLibrary = nullptr;
	DriverTable driver = {};
	CUdevice device = 0;
	CUcontext context = nullptr;
	bool available = false;
	std::string error;
	std::string gpuArchitecture;
	CudaCompilerDriver compiler;
	uint64_t nextModuleId = 1;
	uint64_t nextMemoryId = 1;
	std::unordered_map<uint64_t, CUmodule> modules;
	std::unordered_map<uint64_t, CUdeviceptr> allocations;
	mutable std::mutex mutex;

	Impl()
	{
		initialize();
	}

	~Impl()
	{
		std::lock_guard<std::mutex> lock(mutex);
		if(available)
		{
			makeCurrent();
			for(auto &entry : modules)
			{
				driver.cuModuleUnload(entry.second);
			}
			for(auto &entry : allocations)
			{
				driver.cuMemFree(entry.second);
			}
			driver.cuCtxDestroy(context);
		}

		if(cudaLibrary)
		{
			freeLibrary(cudaLibrary);
		}
	}

	void initialize()
	{
		cudaLibrary = loadLibrary("", kCudaLibraries, "cuInit");
		if(!cudaLibrary)
		{
			error = "failed to load libcuda.so";
			return;
		}

		getFuncAddress(cudaLibrary, "cuInit", &driver.cuInit);
		getFuncAddress(cudaLibrary, "cuDeviceGetCount", &driver.cuDeviceGetCount);
		getFuncAddress(cudaLibrary, "cuDeviceGet", &driver.cuDeviceGet);
		getFuncAddress(cudaLibrary, "cuDeviceGetAttribute", &driver.cuDeviceGetAttribute);
		getFuncAddress(cudaLibrary, "cuCtxCreate_v2", &driver.cuCtxCreate);
		if(!driver.cuCtxCreate)
		{
			getFuncAddress(cudaLibrary, "cuCtxCreate", &driver.cuCtxCreate);
		}
		getFuncAddress(cudaLibrary, "cuCtxDestroy_v2", &driver.cuCtxDestroy);
		if(!driver.cuCtxDestroy)
		{
			getFuncAddress(cudaLibrary, "cuCtxDestroy", &driver.cuCtxDestroy);
		}
		getFuncAddress(cudaLibrary, "cuCtxSetCurrent", &driver.cuCtxSetCurrent);
		getFuncAddress(cudaLibrary, "cuCtxSynchronize", &driver.cuCtxSynchronize);
		getFuncAddress(cudaLibrary, "cuModuleLoad", &driver.cuModuleLoad);
		getFuncAddress(cudaLibrary, "cuModuleUnload", &driver.cuModuleUnload);
		getFuncAddress(cudaLibrary, "cuModuleGetFunction", &driver.cuModuleGetFunction);
		getFuncAddress(cudaLibrary, "cuLaunchKernel", &driver.cuLaunchKernel);
		getFuncAddress(cudaLibrary, "cuMemAlloc_v2", &driver.cuMemAlloc);
		if(!driver.cuMemAlloc)
		{
			getFuncAddress(cudaLibrary, "cuMemAlloc", &driver.cuMemAlloc);
		}
		getFuncAddress(cudaLibrary, "cuMemFree_v2", &driver.cuMemFree);
		if(!driver.cuMemFree)
		{
			getFuncAddress(cudaLibrary, "cuMemFree", &driver.cuMemFree);
		}
		getFuncAddress(cudaLibrary, "cuMemcpyHtoD_v2", &driver.cuMemcpyHtoD);
		if(!driver.cuMemcpyHtoD)
		{
			getFuncAddress(cudaLibrary, "cuMemcpyHtoD", &driver.cuMemcpyHtoD);
		}
		getFuncAddress(cudaLibrary, "cuMemcpyDtoH_v2", &driver.cuMemcpyDtoH);
		if(!driver.cuMemcpyDtoH)
		{
			getFuncAddress(cudaLibrary, "cuMemcpyDtoH", &driver.cuMemcpyDtoH);
		}
		getFuncAddress(cudaLibrary, "cuGetErrorName", &driver.cuGetErrorName);
		getFuncAddress(cudaLibrary, "cuGetErrorString", &driver.cuGetErrorString);

		if(!driver.cuInit || !driver.cuDeviceGetCount || !driver.cuDeviceGet || !driver.cuDeviceGetAttribute ||
		   !driver.cuCtxCreate || !driver.cuCtxDestroy || !driver.cuCtxSetCurrent ||
		   !driver.cuCtxSynchronize || !driver.cuModuleLoad || !driver.cuModuleUnload ||
		   !driver.cuModuleGetFunction || !driver.cuLaunchKernel || !driver.cuMemAlloc ||
		   !driver.cuMemFree || !driver.cuMemcpyHtoD || !driver.cuMemcpyDtoH)
		{
			error = "failed to resolve required CUDA driver entry points";
			return;
		}

		if(!check(driver.cuInit(0), "cuInit"))
		{
			return;
		}

		int deviceCount = 0;
		if(!check(driver.cuDeviceGetCount(&deviceCount), "cuDeviceGetCount"))
		{
			return;
		}
		if(deviceCount <= 0)
		{
			error = "no CUDA device available";
			return;
		}

		if(!check(driver.cuDeviceGet(&device, 0), "cuDeviceGet"))
		{
			return;
		}
		if(!check(driver.cuCtxCreate(&context, 0, device), "cuCtxCreate"))
		{
			return;
		}
		if(!makeCurrent())
		{
			return;
		}

		int major = 0;
		int minor = 0;
		if(!check(driver.cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device), "cuDeviceGetAttribute(major)") ||
		   !check(driver.cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device), "cuDeviceGetAttribute(minor)"))
		{
			return;
		}

		gpuArchitecture = "sm_" + std::to_string(major) + std::to_string(minor);
		available = true;
	}

	bool makeCurrent() const
	{
		return context && check(driver.cuCtxSetCurrent(context), "cuCtxSetCurrent");
	}

	bool check(CUresult result, const char *operation) const
	{
		if(result == CUDA_SUCCESS)
		{
			return true;
		}

		const char *errorName = nullptr;
		const char *errorString = nullptr;
		if(driver.cuGetErrorName)
		{
			driver.cuGetErrorName(result, &errorName);
		}
		if(driver.cuGetErrorString)
		{
			driver.cuGetErrorString(result, &errorString);
		}

		std::string description = operation;
		description += " failed";
		if(errorName)
		{
			description += ": ";
			description += errorName;
		}
		if(errorString)
		{
			description += " (";
			description += errorString;
			description += ")";
		}
		const_cast<Impl *>(this)->error = description;
		return false;
	}
};

void CudaRuntimeAPI::resetGlobalCapture()
{
	gLastModuleSource.clear();
	gLastLaunch = {};
	gLaunchCount = 0;
}

const std::string &CudaRuntimeAPI::globalLastModuleSource()
{
	return gLastModuleSource;
}

const LaunchRecord &CudaRuntimeAPI::globalLastLaunch()
{
	return gLastLaunch;
}

uint32_t CudaRuntimeAPI::globalLaunchCount()
{
	return gLaunchCount;
}

CudaRuntimeAPI::CudaRuntimeAPI()
    : impl(new Impl())
{}

CudaRuntimeAPI::~CudaRuntimeAPI() = default;

bool CudaRuntimeAPI::isAvailable() const
{
	return impl->available;
}

const std::string &CudaRuntimeAPI::initializationError() const
{
	return impl->error;
}

bool CudaRuntimeAPI::isHardwareBacked() const
{
	return impl->available;
}

ModuleHandle CudaRuntimeAPI::createModule(const std::string &sourceOrIR)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	if(!impl->available || !impl->makeCurrent())
	{
		return {};
	}

	dumpCudaSource(sourceOrIR);

	auto compile = impl->compiler.compileToFatbin(sourceOrIR, impl->gpuArchitecture);
	if(!compile.succeeded)
	{
		impl->error = compile.errorMessage;
		return {};
	}

	CUmodule module = nullptr;
	if(!impl->check(impl->driver.cuModuleLoad(&module, compile.modulePath.c_str()), "cuModuleLoad"))
	{
		if(!CudaCompilerDriver::keepArtifacts())
		{
			std::filesystem::remove_all(compile.workingDirectory);
		}
		return {};
	}

	if(!CudaCompilerDriver::keepArtifacts())
	{
		std::filesystem::remove_all(compile.workingDirectory);
	}

	auto handle = ModuleHandle{ impl->nextModuleId++ };
	impl->modules.emplace(handle.id, module);
	gLastModuleSource = sourceOrIR;
	return handle;
}

DeviceMemoryHandle CudaRuntimeAPI::allocateMemory(size_t numBytes)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	if(!impl->available || !impl->makeCurrent())
	{
		return {};
	}

	CUdeviceptr allocation = 0;
	if(!impl->check(impl->driver.cuMemAlloc(&allocation, numBytes), "cuMemAlloc"))
	{
		return {};
	}

	auto handle = DeviceMemoryHandle{ impl->nextMemoryId++ };
	impl->allocations.emplace(handle.id, allocation);
	return handle;
}

void CudaRuntimeAPI::freeMemory(DeviceMemoryHandle memory)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	auto it = impl->allocations.find(memory.id);
	if(it == impl->allocations.end() || !impl->makeCurrent())
	{
		return;
	}

	impl->driver.cuMemFree(it->second);
	impl->allocations.erase(it);
}

void CudaRuntimeAPI::copyHostToMemory(DeviceMemoryHandle memory, const void *source, size_t numBytes)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	auto it = impl->allocations.find(memory.id);
	if(it == impl->allocations.end() || !impl->makeCurrent())
	{
		return;
	}

	impl->check(impl->driver.cuMemcpyHtoD(it->second, source, numBytes), "cuMemcpyHtoD");
}

void CudaRuntimeAPI::copyMemoryToHost(void *destination, DeviceMemoryHandle memory, size_t numBytes)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	auto it = impl->allocations.find(memory.id);
	if(it == impl->allocations.end() || !impl->makeCurrent())
	{
		return;
	}

	impl->check(impl->driver.cuMemcpyDtoH(destination, it->second, numBytes), "cuMemcpyDtoH");
}

uint64_t CudaRuntimeAPI::memoryAddress(DeviceMemoryHandle memory) const
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	auto it = impl->allocations.find(memory.id);
	if(it == impl->allocations.end())
	{
		return 0;
	}

	return static_cast<uint64_t>(it->second);
}

void CudaRuntimeAPI::launch(ModuleHandle moduleHandle, const LaunchRecord &record, const std::vector<void *> &arguments)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	if(!impl->available || !impl->makeCurrent())
	{
		return;
	}

	auto moduleIt = impl->modules.find(moduleHandle.id);
	if(moduleIt == impl->modules.end())
	{
		return;
	}

	CUfunction function = nullptr;
	if(!impl->check(impl->driver.cuModuleGetFunction(&function, moduleIt->second, kKernelName), "cuModuleGetFunction"))
	{
		return;
	}

	std::vector<void *> kernelArguments = arguments;
	if(!impl->check(impl->driver.cuLaunchKernel(
	                    function,
	                    std::max(record.groupCountX, 1u),
	                    std::max(record.groupCountY, 1u),
	                    std::max(record.groupCountZ, 1u),
	                    std::max(record.blockCountX, 1u),
	                    std::max(record.blockCountY, 1u),
	                    std::max(record.blockCountZ, 1u),
	                    0,
	                    nullptr,
	                    kernelArguments.empty() ? nullptr : kernelArguments.data(),
	                    nullptr),
	                "cuLaunchKernel"))
	{
		return;
	}

	if(!impl->check(impl->driver.cuCtxSynchronize(), "cuCtxSynchronize"))
	{
		return;
	}

	gLastLaunch = record;
	gLastLaunch.module = moduleHandle;
	gLastLaunch.argumentCount = arguments.size();
	gLaunchCount++;
	recordLaunchStamp();
}

void CudaRuntimeAPI::synchronize()
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	if(impl->available && impl->makeCurrent())
	{
		impl->check(impl->driver.cuCtxSynchronize(), "cuCtxSynchronize");
	}
}

}  // namespace backend
