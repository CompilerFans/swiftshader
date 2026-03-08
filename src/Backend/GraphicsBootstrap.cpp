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

struct BootstrapVsParams
{
	const BootstrapVertexInput *inVertices = nullptr;
	BootstrapVertexOutput *outVertices = nullptr;
	uint32_t vertexCount = 0;
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

struct VsParams
{
	const VertexInput *inVertices;
	VertexOutput *outVertices;
	unsigned int vertexCount;
};

static __device__ void vs_main(VsParams params, unsigned int vertexIndex, const VertexInput &inVertex, VertexOutput &outVertex)
{
	outVertex.x = inVertex.x;
	outVertex.y = inVertex.y;
	outVertex.z = inVertex.z;
	outVertex.w = 1.0f;
}

static __device__ void run_vs_entry(VsParams params)
{
	unsigned int vertexIndex = blockIdx.x * blockDim.x + threadIdx.x;
	if(vertexIndex >= params.vertexCount)
	{
		return;
	}

	VertexInput inVertex = params.inVertices[vertexIndex];
	VertexOutput outVertex = {};
	vs_main(params, vertexIndex, inVertex, outVertex);
	params.outVertices[vertexIndex] = outVertex;
}

extern "C" __global__ void vs_entry(VsParams params)
{
	run_vs_entry(params);
}
)";
}

void launchGraphicsBootstrap(RuntimeAPI &runtime)
{
	auto module = runtime.createModule(graphicsBootstrapCudaSource(), "vs_entry");
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

	BootstrapVsParams params = {};
	params.inVertices = reinterpret_cast<const BootstrapVertexInput *>(static_cast<uintptr_t>(runtime.memoryAddress(inputMemory)));
	params.outVertices = reinterpret_cast<BootstrapVertexOutput *>(static_cast<uintptr_t>(runtime.memoryAddress(outputMemory)));
	std::vector<void *> arguments = { &params };

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
