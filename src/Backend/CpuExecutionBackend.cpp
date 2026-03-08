#include "Vulkan/VulkanPlatform.hpp"
#include "GraphicsBackend.hpp"

#include "Backend/ResourceStateTracker.hpp"
#include "Device/Renderer.hpp"
#include "Vulkan/VkCommandBuffer.hpp"
#include "Vulkan/VkQueue.hpp"
#include "Vulkan/VkStructConversion.hpp"

namespace backend {
namespace {

class CpuGraphicsBackend : public GraphicsBackend
{
public:
	explicit CpuGraphicsBackend(vk::Device *device)
	    : device(device)
	{}

	sw::Renderer *renderer() override
	{
		ensureRenderer();
		return rendererImpl.get();
	}

	void synchronize() override
	{
		if(rendererImpl)
		{
			rendererImpl->synchronize();
		}
	}

private:
	void ensureRenderer()
	{
		if(!rendererImpl && device)
		{
			rendererImpl.reset(new sw::Renderer(device));
		}
	}

	vk::Device *device = nullptr;
	std::unique_ptr<sw::Renderer> rendererImpl;
};

class CpuExecutionBackend : public ExecutionBackend
{
public:
	explicit CpuExecutionBackend(vk::Device *device)
	    : graphics(std::make_unique<CpuGraphicsBackend>(device))
	{}

	void submit(vk::Device *device, vk::SubmitInfo &submitInfo, sw::CountedEvent *events) override
	{
		vk::CommandBuffer::ExecutionState executionState;
		executionState.renderer = graphics->renderer();
		executionState.executionBackend = this;
		executionState.events = events;
		for(uint32_t j = 0; j < submitInfo.commandBufferCount; j++)
		{
			vk::Cast(submitInfo.pCommandBuffers[j])->submit(executionState);
		}
	}

	void synchronize() override
	{
		graphics->synchronize();
	}

private:
	std::unique_ptr<GraphicsBackend> graphics;
};

}  // namespace

std::unique_ptr<ExecutionBackend> createCpuExecutionBackend(vk::Device *device)
{
	auto capture = lastExecutionBackendCapture();
	(void)capture;
	const_cast<ExecutionBackendCapture &>(lastExecutionBackendCapture()).usedCpuFactory = true;
	return std::make_unique<CpuExecutionBackend>(device);
}

}  // namespace backend
