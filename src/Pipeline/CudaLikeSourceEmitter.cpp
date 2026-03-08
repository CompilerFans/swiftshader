#include "CudaLikeSourceEmitter.hpp"

#include <sstream>

namespace sw {

namespace {

std::string emitFloatLiteral(float value)
{
	std::ostringstream stream;
	stream << value << 'f';
	return stream.str();
}

std::string emitVertexCudaLikeSource(const VertexLoweringInfo &vertex)
{
	std::ostringstream source;
	source << "struct VertexInput\n";
	source << "{\n";
	source << "\tfloat x;\n";
	source << "\tfloat y;\n";
	source << "\tfloat z;\n";
	source << "};\n\n";
	source << "struct VertexOutput\n";
	source << "{\n";
	source << "\tfloat x;\n";
	source << "\tfloat y;\n";
	source << "\tfloat z;\n";
	source << "\tfloat w;\n";
	source << "};\n\n";
	source << "struct VsParams\n";
	source << "{\n";
	source << "\tconst unsigned char *vertexData;\n";
	source << "\tVertexOutput *outVertices;\n";
	source << "\tunsigned int vertexCount;\n";
	source << "\tunsigned int vertexStride;\n";
	source << "\tunsigned int positionOffset;\n";
	source << "\tunsigned int instanceIndex;\n";
	source << "};\n\n";
	source << "static __device__ void vs_main(const VsParams &params, unsigned int vertexIndex, const VertexInput &inVertex, VertexOutput &outVertex)\n";
	source << "{\n";
	source << "\t(void)params;\n";
	source << "\toutVertex.x = inVertex.x;\n";
	source << "\toutVertex.y = inVertex.y;\n";
	source << "\toutVertex.z = inVertex.z;\n";
	source << "\toutVertex.w = 1.0f;\n";
	if(vertex.usesVertexIndex)
	{
		source << "\toutVertex.x += static_cast<float>(vertexIndex);\n";
	}
	if(vertex.usesInstanceIndex)
	{
		source << "\toutVertex.y += static_cast<float>(params.instanceIndex);\n";
	}
	if(vertex.constantOffsetX != 0.0f)
	{
		source << "\toutVertex.x += " << emitFloatLiteral(vertex.constantOffsetX) << ";\n";
	}
	if(vertex.constantOffsetY != 0.0f)
	{
		source << "\toutVertex.y += " << emitFloatLiteral(vertex.constantOffsetY) << ";\n";
	}
	if(vertex.constantOffsetZ != 0.0f)
	{
		source << "\toutVertex.z += " << emitFloatLiteral(vertex.constantOffsetZ) << ";\n";
	}
	source << "}\n\n";
	source << "extern \"C\" __global__ void vs_entry(VsParams params)\n";
	source << "{\n";
	source << "\tunsigned int vertexIndex = blockIdx.x * blockDim.x + threadIdx.x;\n";
	source << "\tif(vertexIndex >= params.vertexCount)\n";
	source << "\t{\n";
	source << "\t\treturn;\n";
	source << "\t}\n";
	if(vertex.usesPositionAttribute)
	{
		source << "\tconst unsigned char *vertexBase = params.vertexData + vertexIndex * params.vertexStride + params.positionOffset;\n";
		source << "\tconst float *position = reinterpret_cast<const float *>(vertexBase);\n";
		source << "\tVertexInput inVertex = { position[0], position[1], position[2] };\n";
	}
	else
	{
		source << "\tVertexInput inVertex = { 0.0f, 0.0f, 0.0f };\n";
	}
	source << "\tVertexOutput outVertex = {};\n";
	source << "\tvs_main(params, vertexIndex, inVertex, outVertex);\n";
	source << "\tparams.outVertices[vertexIndex] = outVertex;\n";
	source << "}\n";
	return source.str();
}

}  // namespace

std::string emitCudaLikeSource(const KernelIRModule &module)
{
	if(module.hasVertexLoweringInfo())
	{
		return emitVertexCudaLikeSource(module.vertexLoweringInfo());
	}
	return "extern \"C\" __global__ void kernel_main() {}\n";
}

NormalizedAbiDescription describeCudaLikeAbi(const KernelIRModule &module)
{
	NormalizedAbiDescription abi = {};
	abi.fragment = module.fragmentExecutionInfo();
	return abi;
}

}  // namespace sw
