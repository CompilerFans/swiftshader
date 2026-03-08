#include "GraphicsBootstrap.hpp"

#include <limits>
#include <sstream>
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

namespace {

std::string literalFloat(float value)
{
	std::ostringstream stream;
	stream.precision(std::numeric_limits<float>::max_digits10);
	stream << value;

	std::string text = stream.str();
	if(text.find('.') == std::string::npos && text.find('e') == std::string::npos && text.find('E') == std::string::npos)
	{
		text += ".0";
	}
	text += 'f';
	return text;
}

}  // namespace

std::string graphicsBootstrapCudaSource(const GraphicsBootstrapShaderConfig &config)
{
	std::ostringstream source;
	source << "struct VertexInput\n"
	          "{\n"
	          "\tfloat x;\n"
	          "\tfloat y;\n"
	          "\tfloat z;\n"
	          "};\n\n"
	          "struct VertexOutput\n"
	          "{\n"
	          "\tfloat x;\n"
	          "\tfloat y;\n"
	          "\tfloat z;\n"
	          "\tfloat w;\n"
	          "};\n\n"
	          "struct VsParams\n"
	          "{\n"
	          "\tconst VertexInput *inVertices;\n"
	          "\tVertexOutput *outVertices;\n"
	          "\tunsigned int vertexCount;\n"
	          "};\n\n"
	          "static __device__ void vs_main(VsParams params, unsigned int vertexIndex, const VertexInput &inVertex, VertexOutput &outVertex)\n"
	          "{\n"
	       << "\toutVertex.x = inVertex.x + " << literalFloat(config.offsetX) << ";\n"
	       << "\toutVertex.y = inVertex.y + " << literalFloat(config.offsetY) << ";\n"
	       << "\toutVertex.z = inVertex.z + " << literalFloat(config.offsetZ) << ";\n"
	          "\toutVertex.w = 1.0f;\n"
	          "}\n\n"
	          "static __device__ void run_vs_entry(VsParams params)\n"
	          "{\n"
	          "\tunsigned int vertexIndex = blockIdx.x * blockDim.x + threadIdx.x;\n"
	          "\tif(vertexIndex >= params.vertexCount)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n\n"
	          "\tVertexInput inVertex = params.inVertices[vertexIndex];\n"
	          "\tVertexOutput outVertex = {};\n"
	          "\tvs_main(params, vertexIndex, inVertex, outVertex);\n"
	          "\tparams.outVertices[vertexIndex] = outVertex;\n"
	          "}\n\n"
	          "extern \"C\" __global__ void vs_entry(VsParams params)\n"
	          "{\n"
	          "\trun_vs_entry(params);\n"
	          "}\n";
	return source.str();
}

bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, std::vector<GraphicsBootstrapVertexOutput> *outputs)
{
	return runGraphicsBootstrap(runtime, inputs, GraphicsBootstrapShaderConfig{}, outputs);
}

bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, std::vector<GraphicsBootstrapVertexOutput> *outputs)
{
	auto module = runtime.createModule(graphicsBootstrapCudaSource(config), "vs_entry");
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
