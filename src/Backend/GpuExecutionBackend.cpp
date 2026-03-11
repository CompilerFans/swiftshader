#include "Vulkan/VulkanPlatform.hpp"
#include "GraphicsBackend.hpp"

#include "Backend/RuntimeAPI.hpp"
#include "Device/Renderer.hpp"
#include "System/Debug.hpp"
#include "Vulkan/VkCommandBuffer.hpp"
#include "Vulkan/VkDevice.hpp"
#include "Vulkan/VkStructConversion.hpp"

#include <cstdio>
#include <cstdlib>

namespace backend {
namespace {

bool shouldTraceGpuSubmit()
{
	const char *value = std::getenv("SWIFTSHADER_GPU_TRACE_GPU_SUBMIT");
	return value != nullptr && value[0] != '\0';
}

class GpuExecutionBackend : public ExecutionBackend
{
public:
	explicit GpuExecutionBackend(vk::Device *device)
	    : runtime(device ? device->getRuntimeAPI() : nullptr)
	{}

	void submit(vk::Device *device, vk::SubmitInfo &submitInfo, sw::CountedEvent *events) override
	{
		if(shouldTraceGpuSubmit())
		{
			std::fprintf(stderr, "[gpu] submit via gpu execution backend\n");
		}

		ensureRenderer(device);

		vk::CommandBuffer::ExecutionState executionState;
		executionState.renderer = renderer.get();
		executionState.executionBackend = this;
		executionState.events = events;
		for(uint32_t j = 0; j < submitInfo.commandBufferCount; j++)
		{
			vk::Cast(submitInfo.pCommandBuffers[j])->submit(executionState);
		}
	}

	void synchronize() override
	{
		if(runtime)
		{
			runtime->synchronize();
		}
		if(renderer)
		{
			renderer->synchronize();
		}
	}

private:
	void ensureRenderer(vk::Device *device)
	{
		if(!renderer && device)
		{
			renderer.reset(new sw::Renderer(device));
		}
	}

	std::unique_ptr<sw::Renderer> renderer;
	RuntimeAPI *runtime = nullptr;
};

}  // namespace

std::unique_ptr<ExecutionBackend> createGpuExecutionBackend(vk::Device *device)
{
	const_cast<ExecutionBackendCapture &>(lastExecutionBackendCapture()).usedGpuFactory = true;
	return std::make_unique<GpuExecutionBackend>(device);
}

}  // namespace backend
