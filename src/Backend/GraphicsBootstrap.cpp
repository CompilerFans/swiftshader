#include "GraphicsBootstrap.hpp"

#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

namespace backend {
namespace {
struct BootstrapVsParams
{
	const uint8_t *vertexData = nullptr;
	GraphicsBootstrapVertexOutput *outVertices = nullptr;
	uint32_t vertexCount = 0;
	uint32_t vertexStride = 0;
	uint32_t positionOffset = 0;
	uint32_t positionComponentCount = 3;
	uint32_t colorOffset = 0;
	uint32_t colorComponentCount = 0;
	uint32_t texCoordOffset = 0;
	uint32_t texCoordComponentCount = 0;
	uint32_t instanceIndex = 0;
	float runtimeOffsetX = 0.0f;
	float runtimeOffsetY = 0.0f;
	float runtimeOffsetZ = 0.0f;
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
	std::string xExpression = "inVertex.x + " + literalFloat(config.offsetX);
	if(config.vertexIndexScaleX != 0.0f)
	{
		xExpression += " + static_cast<float>(vertexIndex) * " + literalFloat(config.vertexIndexScaleX);
	}
	std::string yExpression = "inVertex.y + " + literalFloat(config.offsetY);
	if(config.instanceIndexScaleY != 0.0f)
	{
		yExpression += " + static_cast<float>(params.instanceIndex) * " + literalFloat(config.instanceIndexScaleY);
	}

	std::ostringstream source;
	source << "struct VertexInput\n"
	          "{\n"
	          "\tfloat x;\n"
	          "\tfloat y;\n"
	          "\tfloat z;\n"
	          "\tfloat colorR;\n"
	          "\tfloat colorG;\n"
	          "\tfloat colorB;\n"
	          "\tfloat colorA;\n"
	          "\tfloat u;\n"
	          "\tfloat v;\n"
	          "};\n\n"
	          "struct VertexOutput\n"
	          "{\n"
	          "\tfloat x;\n"
	          "\tfloat y;\n"
	          "\tfloat z;\n"
	          "\tfloat w;\n"
	          "\tfloat pointSize;\n"
	          "\tfloat colorR;\n"
	          "\tfloat colorG;\n"
	          "\tfloat colorB;\n"
	          "\tfloat colorA;\n"
	          "\tfloat u;\n"
	          "\tfloat v;\n"
	          "};\n\n"
	          "struct VsParams\n"
	          "{\n"
	          "\tconst unsigned char *vertexData;\n"
	          "\tVertexOutput *outVertices;\n"
	          "\tunsigned int vertexCount;\n"
	          "\tunsigned int vertexStride;\n"
	          "\tunsigned int positionOffset;\n"
	          "\tunsigned int positionComponentCount;\n"
	          "\tunsigned int colorOffset;\n"
	          "\tunsigned int colorComponentCount;\n"
	          "\tunsigned int texCoordOffset;\n"
	          "\tunsigned int texCoordComponentCount;\n"
	          "\tunsigned int instanceIndex;\n"
	          "\tfloat runtimeOffsetX;\n"
	          "\tfloat runtimeOffsetY;\n"
	          "\tfloat runtimeOffsetZ;\n"
	          "};\n\n"
	          "static __device__ void vs_main(VsParams params, unsigned int vertexIndex, const VertexInput &inVertex, VertexOutput &outVertex)\n"
	          "{\n"
	       << "\toutVertex.x = " << xExpression << " + params.runtimeOffsetX;\n"
	       << "\toutVertex.y = " << yExpression << " + params.runtimeOffsetY;\n"
	       << "\toutVertex.z = inVertex.z + " << literalFloat(config.offsetZ) << " + params.runtimeOffsetZ;\n"
	          "\toutVertex.w = 1.0f;\n"
	       << "\toutVertex.pointSize = " << literalFloat(config.pointSize) << ";\n"
	          "\toutVertex.colorR = inVertex.colorR;\n"
	          "\toutVertex.colorG = inVertex.colorG;\n"
	          "\toutVertex.colorB = inVertex.colorB;\n"
	          "\toutVertex.colorA = inVertex.colorA;\n"
	          "\toutVertex.u = inVertex.u;\n"
	          "\toutVertex.v = inVertex.v;\n"
	          "}\n\n"
	          "static __device__ void run_vs_entry(VsParams params)\n"
	          "{\n"
	          "\tunsigned int vertexIndex = blockIdx.x * blockDim.x + threadIdx.x;\n"
	          "\tif(vertexIndex >= params.vertexCount)\n"
	          "\t{\n"
	          "\t\treturn;\n"
	          "\t}\n\n"
	          "\tconst unsigned char *vertexBase = params.vertexData + vertexIndex * params.vertexStride + params.positionOffset;\n"
	          "\tconst float *position = reinterpret_cast<const float *>(vertexBase);\n"
	          "\tfloat z = params.positionComponentCount > 2 ? position[2] : 0.0f;\n"
	          "\tfloat colorR = 1.0f;\n"
	          "\tfloat colorG = 1.0f;\n"
	          "\tfloat colorB = 1.0f;\n"
	          "\tfloat colorA = 1.0f;\n"
	          "\tfloat texCoordU = 0.0f;\n"
	          "\tfloat texCoordV = 0.0f;\n"
	          "\tif(params.colorComponentCount != 0u)\n"
	          "\t{\n"
	          "\t\tconst float *color = reinterpret_cast<const float *>(params.vertexData + vertexIndex * params.vertexStride + params.colorOffset);\n"
	          "\t\tcolorR = color[0];\n"
	          "\t\tcolorG = params.colorComponentCount > 1 ? color[1] : colorR;\n"
	          "\t\tcolorB = params.colorComponentCount > 2 ? color[2] : colorG;\n"
	          "\t\tcolorA = params.colorComponentCount > 3 ? color[3] : 1.0f;\n"
	          "\t}\n"
	          "\tif(params.texCoordComponentCount != 0u)\n"
	          "\t{\n"
	          "\t\tconst float *texCoord = reinterpret_cast<const float *>(params.vertexData + vertexIndex * params.vertexStride + params.texCoordOffset);\n"
	          "\t\ttexCoordU = texCoord[0];\n"
	          "\t\ttexCoordV = params.texCoordComponentCount > 1 ? texCoord[1] : 0.0f;\n"
	          "\t}\n"
	          "\tVertexInput inVertex = { position[0], position[1], z, colorR, colorG, colorB, colorA, texCoordU, texCoordV };\n"
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
	return runGraphicsBootstrap(runtime, inputs, GraphicsBootstrapShaderConfig{}, GraphicsBootstrapRuntimeConfig{}, outputs);
}

bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, std::vector<GraphicsBootstrapVertexOutput> *outputs)
{
	return runGraphicsBootstrap(runtime, inputs, config, GraphicsBootstrapRuntimeConfig{}, outputs);
}

bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<GraphicsBootstrapVertexInput> &inputs, const GraphicsBootstrapShaderConfig &config, const GraphicsBootstrapRuntimeConfig &runtimeConfig, std::vector<GraphicsBootstrapVertexOutput> *outputs)
{
	std::vector<uint8_t> rawVertexData(sizeof(GraphicsBootstrapVertexInput) * inputs.size());
	if(!rawVertexData.empty())
	{
		std::memcpy(rawVertexData.data(), inputs.data(), rawVertexData.size());
	}

	GraphicsBootstrapBindingConfig bindingConfig = {};
	bindingConfig.vertexStride = sizeof(GraphicsBootstrapVertexInput);
	bindingConfig.positionOffset = 0;
	bindingConfig.positionComponentCount = 3;
	bindingConfig.colorOffset = offsetof(GraphicsBootstrapVertexInput, colorR);
	bindingConfig.colorComponentCount = 4;
	return runGraphicsBootstrap(runtime, rawVertexData, static_cast<uint32_t>(inputs.size()), bindingConfig, config, runtimeConfig, outputs);
}

bool runGraphicsBootstrap(RuntimeAPI &runtime, const std::vector<uint8_t> &rawVertexData, uint32_t vertexCount, const GraphicsBootstrapBindingConfig &bindingConfig, const GraphicsBootstrapShaderConfig &config, const GraphicsBootstrapRuntimeConfig &runtimeConfig, std::vector<GraphicsBootstrapVertexOutput> *outputs)
{
	auto module = runtime.createModule(graphicsBootstrapCudaSource(config), "vs_entry");
	if(!module.valid())
	{
		return false;
	}

	if(vertexCount == 0 || rawVertexData.empty())
	{
		return false;
	}
	if(bindingConfig.positionComponentCount < 2 || bindingConfig.positionComponentCount > 3)
	{
		return false;
	}

	auto inputMemory = runtime.allocateMemory(rawVertexData.size());
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

	runtime.copyHostToMemory(inputMemory, rawVertexData.data(), rawVertexData.size());

	BootstrapVsParams params = {};
	params.vertexData = reinterpret_cast<const uint8_t *>(static_cast<uintptr_t>(runtime.memoryAddress(inputMemory)));
	params.outVertices = reinterpret_cast<GraphicsBootstrapVertexOutput *>(static_cast<uintptr_t>(runtime.memoryAddress(outputMemory)));
	params.vertexCount = vertexCount;
	params.vertexStride = bindingConfig.vertexStride;
	params.positionOffset = bindingConfig.positionOffset;
	params.positionComponentCount = bindingConfig.positionComponentCount;
	params.colorOffset = bindingConfig.colorOffset;
	params.colorComponentCount = bindingConfig.colorComponentCount;
	params.texCoordOffset = bindingConfig.texCoordOffset;
	params.texCoordComponentCount = bindingConfig.texCoordComponentCount;
	params.instanceIndex = runtimeConfig.instanceIndex;
	params.runtimeOffsetX = runtimeConfig.offsetX;
	params.runtimeOffsetY = runtimeConfig.offsetY;
	params.runtimeOffsetZ = runtimeConfig.offsetZ;
	std::vector<void *> arguments = { &params };

	LaunchRecord record = {};
	record.groupCountX = 1;
	record.groupCountY = 1;
	record.groupCountZ = 1;
	record.blockCountX = vertexCount;
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
