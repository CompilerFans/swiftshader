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

bool shouldTraceCustomSubmit()
{
	const char *value = std::getenv("SWIFTSHADER_CUSTOM_GPU_TRACE_CUSTOM_SUBMIT");
	return value != nullptr && value[0] != '\0';
}

class CustomExecutionBackend : public ExecutionBackend
{
public:
	explicit CustomExecutionBackend(vk::Device *device)
	    : runtime(device ? device->getRuntimeAPI() : nullptr)
	{}

	void submit(vk::Device *device, vk::SubmitInfo &submitInfo, sw::CountedEvent *events) override
	{
		if(shouldTraceCustomSubmit())
		{
			std::fprintf(stderr, "[custom-gpu] submit via custom execution backend\n");
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

std::unique_ptr<ExecutionBackend> createCustomExecutionBackend(vk::Device *device)
{
	const_cast<ExecutionBackendCapture &>(lastExecutionBackendCapture()).usedCustomFactory = true;
	return std::make_unique<CustomExecutionBackend>(device);
}

}  // namespace backend
