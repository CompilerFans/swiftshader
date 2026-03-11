#include "Backend/RuntimeAPI.hpp"
#include "GraphicsBootstrap.hpp"
#include "TrianglePipelineBootstrap.hpp"
#include "System/Debug.hpp"
#include "Vulkan/VkDevice.hpp"
#include "GraphicsBackend.hpp"

#include <cstdio>
#include <cstdlib>

namespace backend {
namespace {

bool shouldTraceCpuSubmit()
{
	const char *value = std::getenv("SWIFTSHADER_CUSTOM_GPU_TRACE_CPU_SUBMIT");
	return value != nullptr && value[0] != '\0';
}

bool shouldRequireCustomSubmit()
{
	const char *value = std::getenv("SWIFTSHADER_CUSTOM_GPU_REQUIRE_CUSTOM_SUBMIT");
	return value != nullptr && value[0] != '\0';
}

class CustomExecutionBackend : public ExecutionBackend
{
public:
	explicit CustomExecutionBackend(vk::Device *device)
	    : cpuBackend(createCpuExecutionBackend(device))
	    , runtime(device ? device->getRuntimeAPI() : nullptr)
	{}

	void submit(vk::Device *device, vk::SubmitInfo &submitInfo, sw::CountedEvent *events) override
	{
		if(shouldTraceCpuSubmit())
		{
			std::fprintf(stderr, "[custom-gpu] submit falling back to CPU backend\n");
		}
		if(shouldRequireCustomSubmit())
		{
			UNSUPPORTED("CustomExecutionBackend submit still falls back to CPU backend (set SWIFTSHADER_CUSTOM_GPU_REQUIRE_CUSTOM_SUBMIT=0 to allow fallback)");
		}
		cpuBackend->submit(device, submitInfo, events);
	}

	void synchronize() override
	{
		if(runtime)
		{
			runtime->synchronize();
		}
		cpuBackend->synchronize();
	}

private:
	std::unique_ptr<ExecutionBackend> cpuBackend;
	RuntimeAPI *runtime = nullptr;
};

}  // namespace

std::unique_ptr<ExecutionBackend> createCustomExecutionBackend(vk::Device *device)
{
	const_cast<ExecutionBackendCapture &>(lastExecutionBackendCapture()).usedCustomFactory = true;
	return std::make_unique<CustomExecutionBackend>(device);
}

}  // namespace backend
