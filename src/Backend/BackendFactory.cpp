#include "BackendFactory.hpp"
#include "BackendConfig.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "CudaRuntimeAPI.hpp"
#endif
#include "FakeRuntimeAPI.hpp"
#include "PresentAdapter.hpp"
#include "System/Debug.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace backend {
namespace {

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
bool envEnabled(const char *name)
{
	const char *value = std::getenv(name);
	if(!value || value[0] == '\0')
	{
		return false;
	}

	std::string normalized(value);
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return normalized != "0" && normalized != "false" && normalized != "off" && normalized != "no";
}

bool shouldTraceCudaCalls()
{
	return envEnabled("SWIFTSHADER_CUDA_TRACE_CALLS");
}

void traceCuda(const char *message)
{
	if(!shouldTraceCudaCalls())
	{
		return;
	}

	std::fprintf(stderr, "[cuda] %s\n", message);
	std::fflush(stderr);
}

const char kRuntimeBootstrapKernel[] = R"(extern "C" __global__ void kernel_main()
{
}
)";

void warmupCudaRuntime(RuntimeAPI &runtime)
{
	traceCuda("warmup runtime");
	auto module = runtime.createModule(kRuntimeBootstrapKernel);
	if(!module.valid())
	{
		traceCuda("warmup createModule failed");
		return;
	}

	LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = 1;
	record.blockCountY = 1;
	record.blockCountZ = 1;
	traceCuda("warmup launch");
	runtime.launch(module, record, {});
}
#endif

}  // namespace

BackendKind defaultBackendKind()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return BackendKind::CUSTOM_GPU;
#else
	return BackendKind::CPU;
#endif
}

std::unique_ptr<RuntimeAPI> createRuntimeAPI()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
#	if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
	auto runtime = std::make_unique<CudaRuntimeAPI>();
	if(runtime->isAvailable())
	{
		if(std::getenv("SWIFTSHADER_CUDA_DISABLE_WARMUP") == nullptr)
		{
			warmupCudaRuntime(*runtime);
		}
		else
		{
			traceCuda("warmup disabled via SWIFTSHADER_CUDA_DISABLE_WARMUP");
		}
		return runtime;
	}
	traceCuda(runtime->initializationError().c_str());
	if(!envEnabled("SWIFTSHADER_CUSTOM_GPU_ALLOW_CPU_FALLBACK"))
	{
		sw::abort("CUDA runtime unavailable: %s\n", runtime->initializationError().c_str());
	}
	return nullptr;
#	else
	return std::make_unique<FakeRuntimeAPI>();
#	endif
#else
	return nullptr;
#endif
}

std::unique_ptr<PresentAdapter> createPresentAdapter()
{
#if SWIFTSHADER_ENABLE_CUSTOM_GPU_BACKEND
	return createCustomPresentAdapter();
#else
	return createFallbackPresentAdapter();
#endif
}


}  // namespace backend
