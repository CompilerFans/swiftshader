#include "BackendFactory.hpp"
#include "BackendConfig.hpp"
#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
#	include "CudaRuntimeAPI.hpp"
#endif
#include "FakeRuntimeAPI.hpp"
#include "PresentAdapter.hpp"

#include <cstdlib>

namespace backend {
namespace {

#if SWIFTSHADER_CUSTOM_GPU_USE_CUDA
const char kRuntimeBootstrapKernel[] = R"(extern "C" __global__ void kernel_main()
{
}
)";

void warmupCudaRuntime(RuntimeAPI &runtime)
{
	auto module = runtime.createModule(kRuntimeBootstrapKernel);
	if(!module.valid())
	{
		return;
	}

	LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = 1;
	record.blockCountY = 1;
	record.blockCountZ = 1;
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
		return runtime;
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
