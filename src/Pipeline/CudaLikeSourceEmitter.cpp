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
		uint32_t componentCount = (vertex.positionInputComponentCount == 0) ? 3u : vertex.positionInputComponentCount;
		source << "\tconst unsigned char *vertexBase = params.vertexData + vertexIndex * params.vertexStride + params.positionOffset;\n";
		source << "\tconst float *position = reinterpret_cast<const float *>(vertexBase);\n";
		switch(componentCount)
		{
		case 1:
			source << "\tVertexInput inVertex = { position[0], 0.0f, 0.0f };\n";
			break;
		case 2:
			source << "\tVertexInput inVertex = { position[0], position[1], 0.0f };\n";
			break;
		default:
			source << "\tVertexInput inVertex = { position[0], position[1], position[2] };\n";
			break;
		}
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

void emitCompilerAnalysisPreamble(std::ostringstream &source, const CompilerAnalysisInfo &analysis)
{
	source << "static __device__ constexpr unsigned int swiftshader_fragment_feature_mask = "
	       << analysis.fragmentFeatureMask << "u;\n";
	source << "static __device__ constexpr unsigned int swiftshader_unsupported_reason_mask = "
	       << analysis.unsupportedReasonMask << "u;\n";
	source << "static __device__ constexpr bool swiftshader_has_texture_plan = "
	       << (analysis.hasTexturePlan ? "true" : "false") << ";\n";
	source << "static __device__ constexpr bool swiftshader_has_image_resource_plan = "
	       << (analysis.hasImageResourcePlan ? "true" : "false") << ";\n";
	source << "static __device__ constexpr bool swiftshader_has_resource_plan = "
	       << (analysis.hasResourcePlan ? "true" : "false") << ";\n\n";
}

std::string emitCombinedTextureFragmentCudaLikeSource(const CompilerAnalysisInfo &analysis)
{
	std::ostringstream source;
	emitCompilerAnalysisPreamble(source, analysis);
	source << "struct FragmentInvocation\n";
	source << "{\n";
	source << "\tunsigned int x;\n";
	source << "\tunsigned int y;\n";
	source << "};\n\n";
	source << "struct FsParams\n";
	source << "{\n";
	source << "\tconst FragmentInvocation *invocations;\n";
	source << "\tunsigned char *colorBuffer;\n";
	source << "\tunsigned int invocationCount;\n";
	source << "\tunsigned int width;\n";
	source << "\tunsigned int height;\n";
	source << "\tconst unsigned char *textureData;\n";
	source << "\tunsigned int textureWidth;\n";
	source << "\tunsigned int textureHeight;\n";
	source << "\tunsigned int textureRowPitchTexels;\n";
	source << "\tunsigned int textureFilterLinear;\n";
	source << "\tunsigned int textureAddressModeU;\n";
	source << "\tunsigned int textureAddressModeV;\n";
	source << "};\n\n";
	source << "static __device__ unsigned char packColor(float value)\n";
	source << "{\n";
	source << "\tvalue = value < 0.0f ? 0.0f : value;\n";
	source << "\tvalue = value > 1.0f ? 1.0f : value;\n";
	source << "\treturn static_cast<unsigned char>(value * 255.0f + 0.5f);\n";
	source << "}\n\n";
	source << "static __device__ float applyAddressMode(float coord, unsigned int addressMode)\n";
	source << "{\n";
	source << "\tif(addressMode != 0u)\n";
	source << "\t{\n";
	source << "\t\tcoord = coord - floorf(coord);\n";
	source << "\t\tif(coord < 0.0f) coord += 1.0f;\n";
	source << "\t\treturn coord;\n";
	source << "\t}\n";
	source << "\tcoord = coord < 0.0f ? 0.0f : coord;\n";
	source << "\tcoord = coord > 1.0f ? 1.0f : coord;\n";
	source << "\treturn coord;\n";
	source << "}\n\n";
	source << "static __device__ void fetchTexel(const FsParams &params, int x, int y, float &r, float &g, float &b, float &a)\n";
	source << "{\n";
	source << "\tunsigned int offset = (static_cast<unsigned int>(y) * params.textureRowPitchTexels + static_cast<unsigned int>(x)) * 4u;\n";
	source << "\tr = params.textureData[offset + 0] / 255.0f;\n";
	source << "\tg = params.textureData[offset + 1] / 255.0f;\n";
	source << "\tb = params.textureData[offset + 2] / 255.0f;\n";
	source << "\ta = params.textureData[offset + 3] / 255.0f;\n";
	source << "}\n\n";
	source << "static __device__ void sampleTexture(const FsParams &params, float u, float v, float &r, float &g, float &b, float &a)\n";
	source << "{\n";
	source << "\tu = applyAddressMode(u, params.textureAddressModeU);\n";
	source << "\tv = applyAddressMode(v, params.textureAddressModeV);\n";
	source << "\tfloat fx = u * static_cast<float>(params.textureWidth - 1u);\n";
	source << "\tfloat fy = v * static_cast<float>(params.textureHeight - 1u);\n";
	source << "\tint x = static_cast<int>(fx + 0.5f);\n";
	source << "\tint y = static_cast<int>(fy + 0.5f);\n";
	source << "\tfetchTexel(params, x, y, r, g, b, a);\n";
	source << "}\n\n";
	source << "static __device__ void fs_main(const FsParams &params, const FragmentInvocation &invocation, unsigned char &outR, unsigned char &outG, unsigned char &outB, unsigned char &outA)\n";
	source << "{\n";
	source << "\t(void)invocation;\n";
	source << "\tfloat colorR;\n";
	source << "\tfloat colorG;\n";
	source << "\tfloat colorB;\n";
	source << "\tfloat colorA;\n";
	source << "\tfloat u = 0.0f;\n";
	source << "\tfloat v = 0.0f;\n";
	source << "\tsampleTexture(params, u, v, colorR, colorG, colorB, colorA);\n";
	source << "\toutR = packColor(colorR);\n";
	source << "\toutG = packColor(colorG);\n";
	source << "\toutB = packColor(colorB);\n";
	source << "\toutA = packColor(colorA);\n";
	source << "}\n\n";
	source << "extern \"C\" __global__ void fs_entry(FsParams params)\n";
	source << "{\n";
	source << "\tunsigned int invocationIndex = blockIdx.x * blockDim.x + threadIdx.x;\n";
	source << "\tif(invocationIndex >= params.invocationCount)\n";
	source << "\t{\n";
	source << "\t\treturn;\n";
	source << "\t}\n";
	source << "\tFragmentInvocation invocation = params.invocations[invocationIndex];\n";
	source << "\tunsigned char outR = 0;\n";
	source << "\tunsigned char outG = 0;\n";
	source << "\tunsigned char outB = 0;\n";
	source << "\tunsigned char outA = 0;\n";
	source << "\tfs_main(params, invocation, outR, outG, outB, outA);\n";
	source << "\tunsigned int offset = (invocation.y * params.width + invocation.x) * 4u;\n";
	source << "\tparams.colorBuffer[offset + 0] = outR;\n";
	source << "\tparams.colorBuffer[offset + 1] = outG;\n";
	source << "\tparams.colorBuffer[offset + 2] = outB;\n";
	source << "\tparams.colorBuffer[offset + 3] = outA;\n";
	source << "}\n";
	return source.str();
}

}  // namespace

std::string emitCudaLikeSource(const KernelIRModule &module)
{
	if(module.compilerAnalysisInfo().hasTexturePlan &&
	   module.compilerAnalysisInfo().textureBootstrapSupported &&
	   (module.compilerAnalysisInfo().textureResourceKind == ShaderTextureResourceKind::CombinedImageSampler ||
	    module.compilerAnalysisInfo().textureResourceKind == ShaderTextureResourceKind::SeparateImageSampler))
	{
		return emitCombinedTextureFragmentCudaLikeSource(module.compilerAnalysisInfo());
	}

	std::ostringstream source;
	emitCompilerAnalysisPreamble(source, module.compilerAnalysisInfo());
	if(module.hasVertexLoweringInfo())
	{
		source << emitVertexCudaLikeSource(module.vertexLoweringInfo());
		return source.str();
	}
	source << "extern \"C\" __global__ void kernel_main() {}\n";
	return source.str();
}

NormalizedAbiDescription describeCudaLikeAbi(const KernelIRModule &module)
{
	NormalizedAbiDescription abi = {};
	abi.fragment = module.fragmentExecutionInfo();
	abi.compilerAnalysis = module.compilerAnalysisInfo();
	return abi;
}

}  // namespace sw
