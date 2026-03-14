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
	uint32_t normalOffset = 0;
	uint32_t normalComponentCount = 0;
	uint32_t instanceIndex = 0;
	uint32_t vertexMode = 0;
	float runtimeOffsetX = 0.0f;
	float runtimeOffsetY = 0.0f;
	float runtimeOffsetZ = 0.0f;
	float modelView[16] = {};
	float modelViewProjection[16] = {};
	float normalMatrix[12] = {};
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
	          "\tfloat normalX;\n"
	          "\tfloat normalY;\n"
	          "\tfloat normalZ;\n"
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
	          "\tunsigned int normalOffset;\n"
	          "\tunsigned int normalComponentCount;\n"
	          "\tunsigned int instanceIndex;\n"
	          "\tunsigned int vertexMode;\n"
	          "\tfloat runtimeOffsetX;\n"
	          "\tfloat runtimeOffsetY;\n"
	          "\tfloat runtimeOffsetZ;\n"
	          "\tfloat modelView[16];\n"
	          "\tfloat modelViewProjection[16];\n"
	          "\tfloat normalMatrix[12];\n"
	          "};\n\n"
	          "static __device__ void mulMat4Vec4(const float *matrix, float x, float y, float z, float w, float &outX, float &outY, float &outZ, float &outW)\n"
	          "{\n"
	          "\toutX = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12] * w;\n"
	          "\toutY = matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13] * w;\n"
	          "\toutZ = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14] * w;\n"
	          "\toutW = matrix[3] * x + matrix[7] * y + matrix[11] * z + matrix[15] * w;\n"
	          "}\n\n"
	          "static __device__ void mulMat3Vec3(const float *matrix, float x, float y, float z, float &outX, float &outY, float &outZ)\n"
	          "{\n"
	          "\toutX = matrix[0] * x + matrix[4] * y + matrix[8] * z;\n"
	          "\toutY = matrix[1] * x + matrix[5] * y + matrix[9] * z;\n"
	          "\toutZ = matrix[2] * x + matrix[6] * y + matrix[10] * z;\n"
	          "}\n\n"
	          "static __device__ float dotVec3(float ax, float ay, float az, float bx, float by, float bz)\n"
	          "{\n"
	          "\treturn ax * bx + ay * by + az * bz;\n"
	          "}\n\n"
	          "static __device__ float safeReciprocal(float value)\n"
	          "{\n"
	          "\treturn fabsf(value) > 1.0e-20f ? 1.0f / value : 1.0f;\n"
	          "}\n\n"
	          "static __device__ void normalizeVec3(float &x, float &y, float &z)\n"
	          "{\n"
	          "\tfloat length = sqrtf(x * x + y * y + z * z);\n"
	          "\tif(length > 1.0e-20f)\n"
	          "\t{\n"
	          "\t\tx /= length;\n"
	          "\t\ty /= length;\n"
	          "\t\tz /= length;\n"
	          "\t}\n"
	          "}\n\n"
	          "static __device__ void vs_main(VsParams params, unsigned int vertexIndex, const VertexInput &inVertex, VertexOutput &outVertex)\n"
	          "{\n"
	          "\tif(params.vertexMode == 1u)\n"
	          "\t{\n"
	          "\t\tfloat clipX = 0.0f;\n"
	          "\t\tfloat clipY = 0.0f;\n"
	          "\t\tfloat clipZ = 0.0f;\n"
	          "\t\tfloat clipW = 1.0f;\n"
	          "\t\tmulMat4Vec4(params.modelViewProjection, inVertex.x, inVertex.y, inVertex.z, 1.0f, clipX, clipY, clipZ, clipW);\n"
	          "\t\tfloat viewX = 0.0f;\n"
	          "\t\tfloat viewY = 0.0f;\n"
	          "\t\tfloat viewZ = 0.0f;\n"
	          "\t\tfloat viewW = 1.0f;\n"
	          "\t\tmulMat4Vec4(params.modelView, inVertex.x, inVertex.y, inVertex.z, 1.0f, viewX, viewY, viewZ, viewW);\n"
	          "\t\tfloat normalX = inVertex.normalX;\n"
	          "\t\tfloat normalY = inVertex.normalY;\n"
	          "\t\tfloat normalZ = inVertex.normalZ;\n"
	          "\t\tmulMat3Vec3(params.normalMatrix, normalX, normalY, normalZ, normalX, normalY, normalZ);\n"
	          "\t\tfloat reciprocalW = safeReciprocal(viewW);\n"
	          "\t\tfloat positionX = viewX * reciprocalW;\n"
	          "\t\tfloat positionY = viewY * reciprocalW;\n"
	          "\t\tfloat positionZ = viewZ * reciprocalW;\n"
	          "\t\tfloat lightDirX = 2.0f - positionX;\n"
	          "\t\tfloat lightDirY = 2.0f - positionY;\n"
	          "\t\tfloat lightDirZ = 20.0f - positionZ;\n"
	          "\t\tnormalizeVec3(lightDirX, lightDirY, lightDirZ);\n"
	          "\t\tfloat diffuse = dotVec3(normalX, normalY, normalZ, lightDirX, lightDirY, lightDirZ);\n"
	          "\t\tdiffuse = diffuse < 0.0f ? 0.0f : diffuse;\n"
	          "\t\toutVertex.x = clipX;\n"
	          "\t\toutVertex.y = clipY;\n"
	          "\t\toutVertex.z = clipZ;\n"
	          "\t\toutVertex.w = clipW;\n"
	          "\t\toutVertex.colorR = inVertex.colorR * diffuse;\n"
	          "\t\toutVertex.colorG = inVertex.colorG * diffuse;\n"
	          "\t\toutVertex.colorB = inVertex.colorB * diffuse;\n"
	          "\t\toutVertex.colorA = 1.0f;\n"
	          "\t}\n"
	          "\telse\n"
	          "\t{\n"
	       << "\t\toutVertex.x = " << xExpression << " + params.runtimeOffsetX;\n"
	       << "\t\toutVertex.y = " << yExpression << " + params.runtimeOffsetY;\n"
	       << "\t\toutVertex.z = inVertex.z + " << literalFloat(config.offsetZ) << " + params.runtimeOffsetZ;\n"
	          "\t\toutVertex.w = 1.0f;\n"
	          "\t\toutVertex.colorR = inVertex.colorR;\n"
	          "\t\toutVertex.colorG = inVertex.colorG;\n"
	          "\t\toutVertex.colorB = inVertex.colorB;\n"
	          "\t\toutVertex.colorA = inVertex.colorA;\n"
	          "\t}\n"
	       << "\toutVertex.pointSize = " << literalFloat(config.pointSize) << ";\n"
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
	          "\tfloat normalX = 0.0f;\n"
	          "\tfloat normalY = 0.0f;\n"
	          "\tfloat normalZ = 1.0f;\n"
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
	          "\tif(params.normalComponentCount != 0u)\n"
	          "\t{\n"
	          "\t\tconst float *normal = reinterpret_cast<const float *>(params.vertexData + vertexIndex * params.vertexStride + params.normalOffset);\n"
	          "\t\tnormalX = normal[0];\n"
	          "\t\tnormalY = params.normalComponentCount > 1 ? normal[1] : 0.0f;\n"
	          "\t\tnormalZ = params.normalComponentCount > 2 ? normal[2] : 1.0f;\n"
	          "\t}\n"
	          "\tVertexInput inVertex = { position[0], position[1], z, colorR, colorG, colorB, colorA, texCoordU, texCoordV, normalX, normalY, normalZ };\n"
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
	bindingConfig.texCoordOffset = offsetof(GraphicsBootstrapVertexInput, u);
	bindingConfig.texCoordComponentCount = 2;
	bindingConfig.normalOffset = offsetof(GraphicsBootstrapVertexInput, normalX);
	bindingConfig.normalComponentCount = 3;
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
	params.normalOffset = bindingConfig.normalOffset;
	params.normalComponentCount = bindingConfig.normalComponentCount;
	params.instanceIndex = runtimeConfig.instanceIndex;
	params.vertexMode = static_cast<uint32_t>(runtimeConfig.vertexMode);
	params.runtimeOffsetX = runtimeConfig.offsetX;
	params.runtimeOffsetY = runtimeConfig.offsetY;
	params.runtimeOffsetZ = runtimeConfig.offsetZ;
	std::memcpy(params.modelView, runtimeConfig.modelView, sizeof(params.modelView));
	std::memcpy(params.modelViewProjection, runtimeConfig.modelViewProjection, sizeof(params.modelViewProjection));
	std::memcpy(params.normalMatrix, runtimeConfig.normalMatrix, sizeof(params.normalMatrix));
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
