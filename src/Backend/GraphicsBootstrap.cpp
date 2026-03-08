#include "GraphicsBootstrap.hpp"

#include <vector>

namespace backend {
namespace {

struct BootstrapVertexInput
{
	float position[3];
};

struct BootstrapVertexOutput
{
	float position[4];
};

}  // namespace

std::string graphicsBootstrapCudaSource()
{
	return R"(struct VertexInput
{
	float x;
	float y;
	float z;
};

struct VertexOutput
{
	float x;
	float y;
	float z;
	float w;
};

extern "C" __global__ void kernel_main(const VertexInput *inVertices, VertexOutput *outVertices, unsigned int vertexCount)
{
	unsigned int vertexIndex = blockIdx.x * blockDim.x + threadIdx.x;
	// vertex bootstrap
	if(vertexIndex >= vertexCount)
	{
		return;
	}

	VertexInput inPos = inVertices[vertexIndex];
	outVertices[vertexIndex].x = inPos.x;
	outVertices[vertexIndex].y = inPos.y;
	outVertices[vertexIndex].z = inPos.z;
	outVertices[vertexIndex].w = 1.0f;
}
)";
}

void launchGraphicsBootstrap(RuntimeAPI &runtime)
{
	auto module = runtime.createModule(graphicsBootstrapCudaSource());
	if(!module.valid())
	{
		return;
	}

	auto inputMemory = runtime.allocateMemory(sizeof(BootstrapVertexInput));
	auto outputMemory = runtime.allocateMemory(sizeof(BootstrapVertexOutput));
	if(!inputMemory.valid() || !outputMemory.valid())
	{
		if(outputMemory.valid())
		{
			runtime.freeMemory(outputMemory);
		}
		if(inputMemory.valid())
		{
			runtime.freeMemory(inputMemory);
		}
		return;
	}

	BootstrapVertexInput input = {};
	runtime.copyHostToMemory(inputMemory, &input, sizeof(input));

	uint64_t inputPointer = runtime.memoryAddress(inputMemory);
	uint64_t outputPointer = runtime.memoryAddress(outputMemory);
	uint32_t vertexCount = 0;
	std::vector<void *> arguments = { &inputPointer, &outputPointer, &vertexCount };

	LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = 1;
	record.blockCountY = 1;
	record.blockCountZ = 1;
	record.argumentCount = arguments.size();
	runtime.launch(module, record, arguments);

	runtime.freeMemory(outputMemory);
	runtime.freeMemory(inputMemory);
}

}  // namespace backend
