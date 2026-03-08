#include "Backend/RuntimeAPI.hpp"
#include "GraphicsBootstrap.hpp"
#include "TrianglePipelineBootstrap.hpp"
#include "Vulkan/VkDevice.hpp"
#include "GraphicsBackend.hpp"

namespace backend {
namespace {

class CustomExecutionBackend : public ExecutionBackend
{
public:
	explicit CustomExecutionBackend(vk::Device *device)
	    : cpuBackend(createCpuExecutionBackend(device))
	    , runtime(device ? device->getRuntimeAPI() : nullptr)
	{}

	void submit(vk::Device *device, vk::SubmitInfo &submitInfo, sw::CountedEvent *events) override
	{
		bootstrapGraphicsPath();
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
	void bootstrapGraphicsPath()
	{
		if(graphicsBootstrapDone || runtime == nullptr)
		{
			return;
		}

		graphicsBootstrapDone = true;
		launchTrianglePipelineBootstrap(*runtime);
	}

	std::unique_ptr<ExecutionBackend> cpuBackend;
	RuntimeAPI *runtime = nullptr;
	bool graphicsBootstrapDone = false;
};

}  // namespace

std::unique_ptr<ExecutionBackend> createCustomExecutionBackend(vk::Device *device)
{
	const_cast<ExecutionBackendCapture &>(lastExecutionBackendCapture()).usedCustomFactory = true;
	return std::make_unique<CustomExecutionBackend>(device);
}

}  // namespace backend
