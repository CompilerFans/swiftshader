#include "GraphicsBootstrap.hpp"

#include <vector>

namespace backend {
namespace {
struct BootstrapVsParams
{
	const GraphicsBootstrapVertexInput *inVertices = nullptr;
	GraphicsBootstrapVertexOutput *outVertices = nullptr;
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

bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, std::vector<GraphicsBootstrapVertexOutput> *outputs)
{
	auto module = runtime.createModule(graphicsBootstrapCudaSource(), "vs_entry");
	if(!module.valid())
	{
		return false;
	}

	size_t vertexCount = inputs.size();
	if(vertexCount == 0)
	{
		return false;
	}

	auto inputMemory = runtime.allocateMemory(sizeof(GraphicsBootstrapVertexInput) * vertexCount);
	auto outputMemory = runtime.allocateMemory(sizeof(GraphicsBootstrapVertexOutput) * vertexCount);
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
		return false;
	}

	runtime.copyHostToMemory(inputMemory, inputs.data(), sizeof(GraphicsBootstrapVertexInput) * vertexCount);

	BootstrapVsParams params = {};
	params.inVertices = reinterpret_cast<const GraphicsBootstrapVertexInput *>(static_cast<uintptr_t>(runtime.memoryAddress(inputMemory)));
	params.outVertices = reinterpret_cast<GraphicsBootstrapVertexOutput *>(static_cast<uintptr_t>(runtime.memoryAddress(outputMemory)));
	params.vertexCount = static_cast<uint32_t>(vertexCount);
	std::vector<void *> arguments = { &params };

	LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = static_cast<uint32_t>(vertexCount);
	record.blockCountY = 1;
	record.blockCountZ = 1;
	record.argumentCount = arguments.size();
	runtime.launch(module, record, arguments);
	runtime.synchronize();

	if(outputs)
	{
		outputs->resize(vertexCount);
		runtime.copyMemoryToHost(outputs->data(), outputMemory, sizeof(GraphicsBootstrapVertexOutput) * vertexCount);
	}

	runtime.freeMemory(outputMemory);
	runtime.freeMemory(inputMemory);
	return true;
}

void launchGraphicsBootstrap(RuntimeAPI &runtime)
{
	static const std::vector<GraphicsBootstrapVertexInput> kBootstrapTriangle = {
		{ -0.5f, -0.25f, 0.0f },
		{ 0.0f, 0.75f, 0.0f },
		{ 0.5f, -0.25f, 0.0f },
	};

	runGraphicsBootstrap(runtime, kBootstrapTriangle, nullptr);
}

}  // namespace backend
