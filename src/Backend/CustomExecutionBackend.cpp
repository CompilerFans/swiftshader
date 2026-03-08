#include "Backend/RuntimeAPI.hpp"
#include "Vulkan/VkDevice.hpp"
#include "GraphicsBackend.hpp"

namespace backend {
namespace {

const char kGraphicsBootstrapKernel[] = R"(extern "C" __global__ void kernel_main()
{
	unsigned int vertexIndex = blockIdx.x * blockDim.x + threadIdx.x;
	// vertex bootstrap
	if(vertexIndex == 0)
	{
		return;
	}
}
)";

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
		auto module = runtime->createModule(kGraphicsBootstrapKernel);
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
		runtime->launch(module, record, {});
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
