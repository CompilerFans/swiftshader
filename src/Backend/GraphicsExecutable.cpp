#include "GraphicsExecutable.hpp"

#include "Pipeline/SemanticIR.hpp"
#include "Pipeline/ShaderCompiler/ShaderCompilerAnalysis.hpp"
#include "Pipeline/SpirvShader.hpp"
#include "System/Memory.hpp"

#include <unordered_set>
#include <vector>

namespace {

bool isBufferDescriptorType(VkDescriptorType descriptorType)
{
	switch(descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return true;
	default:
		return false;
	}
}

backend::GraphicsExecutableDescriptorRef toBackendDescriptorRef(const sw::ShaderDescriptorRef &ref)
{
	backend::GraphicsExecutableDescriptorRef out = {};
	out.descriptorType = ref.descriptorType;
	out.descriptorCount = ref.descriptorCount;
	out.descriptorSet = ref.descriptorSet;
	out.binding = ref.binding;
	out.arrayElement = ref.arrayElement;
	out.hasNonConstantArrayElement = ref.hasNonConstantArrayElement;
	out.isDynamic = ref.isDynamic;
	out.dynamicOffsetIndex = ref.dynamicOffsetIndex;
	return out;
}

backend::GraphicsExecutableImageResourcePlan toBackendImageResourcePlan(const sw::ShaderImageResourcePlan &plan)
{
	backend::GraphicsExecutableImageResourcePlan out = {};
	for(const auto &ref : plan.sampledDescriptors)
	{
		out.sampledDescriptors.push_back(toBackendDescriptorRef(ref));
	}
	for(const auto &ref : plan.storageDescriptors)
	{
		out.storageDescriptors.push_back(toBackendDescriptorRef(ref));
	}
	return out;
}

backend::GraphicsExecutableTexturePlan toBackendTexturePlan(const sw::ShaderTexturePlan &plan)
{
	backend::GraphicsExecutableTexturePlan out = {};
	switch(plan.resourceKind)
	{
	case sw::ShaderTextureResourceKind::CombinedImageSampler:
		out.resourceKind = backend::GraphicsExecutableTextureResourceKind::CombinedImageSampler;
		break;
	case sw::ShaderTextureResourceKind::SeparateImageSampler:
		out.resourceKind = backend::GraphicsExecutableTextureResourceKind::SeparateImageSampler;
		break;
	case sw::ShaderTextureResourceKind::Other:
		out.resourceKind = backend::GraphicsExecutableTextureResourceKind::Other;
		break;
	default:
		out.resourceKind = backend::GraphicsExecutableTextureResourceKind::None;
		break;
	}

	out.imageDescriptorSet = plan.imageDescriptorSet;
	out.imageBinding = plan.imageBinding;
	out.imageArrayElement = plan.imageArrayElement;
	out.samplerDescriptorSet = plan.samplerDescriptorSet;
	out.samplerBinding = plan.samplerBinding;
	out.samplerArrayElement = plan.samplerArrayElement;
	out.bootstrapSupported = plan.bootstrapSupported;
	return out;
}

backend::GraphicsExecutableResourcePlan toBackendResourcePlan(const sw::ShaderResourcePlan &plan)
{
	backend::GraphicsExecutableResourcePlan out = {};
	out.descriptorSetCount = plan.descriptorSetCount;
	out.dynamicOffsetCount = plan.dynamicOffsetCount;
	out.pushConstantSize = plan.pushConstantSize;
	for(const auto &ref : plan.descriptors)
	{
		out.descriptors.push_back(toBackendDescriptorRef(ref));
	}
	return out;
}

uint32_t toBackendFragmentFeatureMask(uint32_t featureMask)
{
	uint32_t mask = 0;
	if((featureMask & static_cast<uint32_t>(sw::ShaderFragmentFeature::Discard)) != 0)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Discard);
	}
	if((featureMask & static_cast<uint32_t>(sw::ShaderFragmentFeature::StorageImageReadWrite)) != 0)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::StorageImageReadWrite);
	}
	if((featureMask & static_cast<uint32_t>(sw::ShaderFragmentFeature::ImageQueryOrFetch)) != 0)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::ImageQueryOrFetch);
	}
	if((featureMask & static_cast<uint32_t>(sw::ShaderFragmentFeature::Derivatives)) != 0)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Derivatives);
	}
	if((featureMask & static_cast<uint32_t>(sw::ShaderFragmentFeature::Atomics)) != 0)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Atomics);
	}
	if((featureMask & static_cast<uint32_t>(sw::ShaderFragmentFeature::Subgroup)) != 0)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Subgroup);
	}
	return mask;
}

struct ShaderAnalysisQueryContext
{
	backend::GraphicsExecutableQueryDescriptorBindingInfo query = nullptr;
	const void *userdata = nullptr;
};

bool queryDescriptorBindingInfoForAnalysis(const void *userdata,
                                          uint32_t descriptorSet,
                                          uint32_t binding,
                                          sw::ShaderDescriptorBindingInfo *bindingInfo)
{
	if(userdata == nullptr || bindingInfo == nullptr)
	{
		return false;
	}

	const auto *queryContext = reinterpret_cast<const ShaderAnalysisQueryContext *>(userdata);
	if(queryContext->query == nullptr)
	{
		return false;
	}

	backend::GraphicsExecutableDescriptorBindingInfo backendInfo = {};
	if(!queryContext->query(queryContext->userdata, descriptorSet, binding, &backendInfo))
	{
		return false;
	}

	bindingInfo->descriptorType = backendInfo.descriptorType;
	bindingInfo->descriptorCount = backendInfo.descriptorCount;
	bindingInfo->isDynamic = backendInfo.isDynamic;
	bindingInfo->dynamicOffsetIndex = backendInfo.dynamicOffsetIndex;
	return true;
}

bool tryGetBootstrapVertexPointSizeConstant(const sw::SpirvShader &shader, float *pointSize)
{
	if(pointSize == nullptr)
	{
		return false;
	}
	auto builtinIt = shader.outputBuiltins.find(spv::BuiltInPointSize);
	if(builtinIt == shader.outputBuiltins.end())
	{
		return false;
	}

	auto builtinId = builtinIt->second.Id;
	auto objectType = shader.getObjectType(builtinId);
	std::unordered_set<uint32_t> candidatePointers = { builtinId.value() };
	if(objectType.isBuiltInBlock)
	{
		auto memberIt = shader.memberDecorations.find(objectType.element);
		if(memberIt == shader.memberDecorations.end())
		{
			return false;
		}
		int pointSizeMember = -1;
		for(size_t memberIndex = 0; memberIndex < memberIt->second.size(); memberIndex++)
		{
			if(memberIt->second[memberIndex].HasBuiltIn &&
			   memberIt->second[memberIndex].BuiltIn == spv::BuiltInPointSize)
			{
				pointSizeMember = static_cast<int>(memberIndex);
				break;
			}
		}
		if(pointSizeMember < 0)
		{
			return false;
		}
		for(auto insn : shader)
		{
			if((insn.opcode() == spv::OpAccessChain || insn.opcode() == spv::OpInBoundsAccessChain) &&
			   insn.word(3) == builtinId.value() && insn.wordCount() >= 5)
			{
				auto indexObject = shader.getObject(sw::Spirv::Object::ID(insn.word(4)));
				if(indexObject.kind == sw::Spirv::Object::Kind::Constant &&
				   !indexObject.constantValue.empty() &&
				   static_cast<int>(indexObject.constantValue[0]) == pointSizeMember)
				{
					candidatePointers.insert(insn.word(2));
				}
			}
		}
	}

	bool sawConstantStore = false;
	float constantPointSize = 0.0f;
	for(auto insn : shader)
	{
		if(insn.opcode() != spv::OpStore || candidatePointers.count(insn.word(1)) == 0)
		{
			continue;
		}
		auto valueObject = shader.getObject(sw::Spirv::Object::ID(insn.word(2)));
		if(valueObject.kind != sw::Spirv::Object::Kind::Constant || valueObject.constantValue.empty())
		{
			return false;
		}
		float storedPointSize = sw::bit_cast<float>(valueObject.constantValue[0]);
		if(sawConstantStore && constantPointSize != storedPointSize)
		{
			return false;
		}
		constantPointSize = storedPointSize;
		sawConstantStore = true;
	}
	if(!sawConstantStore)
	{
		return false;
	}
	*pointSize = constantPointSize;
	return true;
}

bool tryGetBootstrapFragmentConstantColor(const sw::SpirvShader &shader, backend::FragmentBootstrapConfig *config)
{
	uint32_t locationZeroOutputId = 0;
	for(auto insn : shader)
	{
		if(insn.opcode() == spv::OpVariable &&
		   static_cast<spv::StorageClass>(insn.word(3)) == spv::StorageClassOutput)
		{
			auto objectId = sw::Spirv::Object::ID(insn.word(2));
			auto decorationsIt = shader.decorations.find(objectId);
			if(decorationsIt != shader.decorations.end() &&
			   decorationsIt->second.HasLocation &&
			   !decorationsIt->second.HasBuiltIn &&
			   decorationsIt->second.Location == 0)
			{
				if(locationZeroOutputId != 0)
				{
					return false;
				}
				locationZeroOutputId = objectId.value();
			}
		}
	}

	if(locationZeroOutputId == 0)
	{
		return false;
	}

	bool sawConstantStore = false;
	float color[4] = {};
	for(auto insn : shader)
	{
		if(insn.opcode() == spv::OpStore && insn.word(1) == locationZeroOutputId)
		{
			auto valueId = sw::Spirv::Object::ID(insn.word(2));
			const auto &valueObject = shader.getObject(valueId);
			if(valueObject.kind != sw::Spirv::Object::Kind::Constant)
			{
				return false;
			}

			const auto &valueType = shader.getType(valueObject);
			if(valueType.componentCount != 4 || valueObject.constantValue.size() < 4)
			{
				return false;
			}

			float storedColor[4] = {
				sw::bit_cast<float>(valueObject.constantValue[0]),
				sw::bit_cast<float>(valueObject.constantValue[1]),
				sw::bit_cast<float>(valueObject.constantValue[2]),
				sw::bit_cast<float>(valueObject.constantValue[3]),
			};
			if(sawConstantStore)
			{
				for(int component = 0; component < 4; component++)
				{
					if(color[component] != storedColor[component])
					{
						return false;
					}
				}
			}
			else
			{
				for(int component = 0; component < 4; component++)
				{
					color[component] = storedColor[component];
				}
				sawConstantStore = true;
			}
		}
	}

	if(!sawConstantStore)
	{
		return false;
	}

	config->shaderKind = backend::FragmentBootstrapShaderKind::ConstantColor;
	config->colorR = color[0];
	config->colorG = color[1];
	config->colorB = color[2];
	config->colorA = color[3];
	return true;
}

bool isBootstrapTextureSamplingOpcode(spv::Op opcode)
{
	switch(opcode)
	{
	case spv::OpImageSampleImplicitLod:
	case spv::OpImageSampleExplicitLod:
	case spv::OpImageSampleProjImplicitLod:
	case spv::OpImageSampleProjExplicitLod:
		return true;
	default:
		return false;
	}
}

bool shaderContainsTextureSampling(const sw::SpirvShader &shader)
{
	for(auto insn : shader)
	{
		if(isBootstrapTextureSamplingOpcode(insn.opcode()))
		{
			return true;
		}
	}
	return false;
}

bool shaderContainsStorageImageReadWrite(const sw::SpirvShader &shader)
{
	for(auto insn : shader)
	{
		if(insn.opcode() == spv::OpImageRead || insn.opcode() == spv::OpImageWrite)
		{
			return true;
		}
	}
	return false;
}

[[maybe_unused]] uint32_t fragmentFeatureMaskForShader(const sw::SpirvShader &shader)
{
	uint32_t mask = 0;

	if(shader.getAnalysis().ContainsDiscard)
	{
		mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Discard);
	}

	for(auto insn : shader)
	{
		switch(insn.opcode())
		{
		case spv::OpImageRead:
		case spv::OpImageWrite:
			mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::StorageImageReadWrite);
			break;

		case spv::OpImageFetch:
		case spv::OpImageQuerySize:
		case spv::OpImageQuerySizeLod:
		case spv::OpImageQueryLod:
		case spv::OpImageQueryLevels:
		case spv::OpImageQuerySamples:
			mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::ImageQueryOrFetch);
			break;

		case spv::OpDPdx:
		case spv::OpDPdy:
		case spv::OpDPdxFine:
		case spv::OpDPdyFine:
		case spv::OpDPdxCoarse:
		case spv::OpDPdyCoarse:
		case spv::OpFwidth:
		case spv::OpFwidthFine:
		case spv::OpFwidthCoarse:
			mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Derivatives);
			break;

		case spv::OpAtomicLoad:
		case spv::OpAtomicStore:
		case spv::OpAtomicExchange:
		case spv::OpAtomicCompareExchange:
		case spv::OpAtomicCompareExchangeWeak:
		case spv::OpAtomicIIncrement:
		case spv::OpAtomicIDecrement:
		case spv::OpAtomicIAdd:
		case spv::OpAtomicISub:
		case spv::OpAtomicUMin:
		case spv::OpAtomicUMax:
		case spv::OpAtomicAnd:
		case spv::OpAtomicOr:
		case spv::OpAtomicXor:
		case spv::OpAtomicFlagTestAndSet:
		case spv::OpAtomicFlagClear:
			mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Atomics);
			break;

		case spv::OpGroupNonUniformElect:
		case spv::OpGroupNonUniformAll:
		case spv::OpGroupNonUniformAny:
		case spv::OpGroupNonUniformAllEqual:
		case spv::OpGroupNonUniformBallot:
		case spv::OpGroupNonUniformInverseBallot:
		case spv::OpGroupNonUniformBallotBitExtract:
		case spv::OpGroupNonUniformBallotBitCount:
		case spv::OpGroupNonUniformBallotFindLSB:
		case spv::OpGroupNonUniformBallotFindMSB:
		case spv::OpGroupNonUniformShuffle:
		case spv::OpGroupNonUniformShuffleXor:
		case spv::OpGroupNonUniformShuffleUp:
		case spv::OpGroupNonUniformShuffleDown:
		case spv::OpGroupNonUniformIAdd:
		case spv::OpGroupNonUniformFAdd:
		case spv::OpGroupNonUniformIMul:
		case spv::OpGroupNonUniformFMul:
		case spv::OpGroupNonUniformSMin:
		case spv::OpGroupNonUniformUMin:
		case spv::OpGroupNonUniformFMin:
		case spv::OpGroupNonUniformSMax:
		case spv::OpGroupNonUniformUMax:
		case spv::OpGroupNonUniformFMax:
		case spv::OpGroupNonUniformBitwiseAnd:
		case spv::OpGroupNonUniformBitwiseOr:
		case spv::OpGroupNonUniformBitwiseXor:
		case spv::OpGroupNonUniformLogicalAnd:
		case spv::OpGroupNonUniformLogicalOr:
		case spv::OpGroupNonUniformLogicalXor:
		case spv::OpGroupNonUniformQuadBroadcast:
		case spv::OpGroupNonUniformQuadSwap:
			mask |= static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Subgroup);
			break;

		default:
			break;
		}
	}

	return mask;
}

bool tryGetLocationZeroOutputId(const sw::SpirvShader &shader, uint32_t *locationZeroOutputId)
{
	if(locationZeroOutputId == nullptr)
	{
		return false;
	}

	uint32_t outputId = 0;
	for(auto insn : shader)
	{
		if(insn.opcode() != spv::OpVariable ||
		   static_cast<spv::StorageClass>(insn.word(3)) != spv::StorageClassOutput)
		{
			continue;
		}

		auto objectId = sw::Spirv::Object::ID(insn.word(2));
		auto decorationsIt = shader.decorations.find(objectId);
		if(decorationsIt == shader.decorations.end() ||
		   !decorationsIt->second.HasLocation ||
		   decorationsIt->second.HasBuiltIn ||
		   decorationsIt->second.Location != 0)
		{
			continue;
		}

		if(outputId != 0)
		{
			return false;
		}
		outputId = objectId.value();
	}

	if(outputId == 0)
	{
		return false;
	}

	*locationZeroOutputId = outputId;
	return true;
}

bool tryFindInstructionByResultId(const sw::SpirvShader &shader,
                                  sw::Spirv::Object::ID resultId,
                                  sw::SpirvShader::InsnIterator *instruction)
{
	if(instruction == nullptr)
	{
		return false;
	}

	for(auto insn : shader)
	{
		if(insn.hasResultAndType() && insn.resultId() == resultId)
		{
			*instruction = insn;
			return true;
		}
	}
	return false;
}

bool tryResolveFunctionLocalStoredValue(const sw::SpirvShader &shader,
                                        sw::Spirv::Object::ID pointerId,
                                        sw::Spirv::Object::ID *storedValueId)
{
	if(storedValueId == nullptr)
	{
		return false;
	}

	sw::SpirvShader::InsnIterator definingInstruction;
	if(!tryFindInstructionByResultId(shader, pointerId, &definingInstruction) ||
	   definingInstruction.opcode() != spv::OpVariable ||
	   static_cast<spv::StorageClass>(definingInstruction.word(3)) != spv::StorageClassFunction)
	{
		return false;
	}

	bool sawStore = false;
	sw::Spirv::Object::ID resolvedStoredValueId;
	for(auto insn : shader)
	{
		if(insn.opcode() != spv::OpStore || insn.word(1) != pointerId.value())
		{
			continue;
		}

		sw::Spirv::Object::ID candidateValueId(insn.word(2));
		if(sawStore)
		{
			if(resolvedStoredValueId != candidateValueId)
			{
				return false;
			}
			continue;
		}

		resolvedStoredValueId = candidateValueId;
		sawStore = true;
	}

	if(!sawStore)
	{
		return false;
	}

	*storedValueId = resolvedStoredValueId;
	return true;
}

bool valueResolvesToTextureSample(const sw::SpirvShader &shader,
                                  sw::Spirv::Object::ID valueId,
                                  int recursionDepth = 0)
{
	if(recursionDepth > 8)
	{
		return false;
	}

	sw::SpirvShader::InsnIterator definingInstruction;
	if(!tryFindInstructionByResultId(shader, valueId, &definingInstruction))
	{
		return false;
	}

	if(isBootstrapTextureSamplingOpcode(definingInstruction.opcode()))
	{
		return true;
	}

	if((definingInstruction.opcode() == spv::OpCopyObject || definingInstruction.opcode() == spv::OpCopyLogical) &&
	   definingInstruction.wordCount() >= 4)
	{
		return valueResolvesToTextureSample(shader, sw::Spirv::Object::ID(definingInstruction.word(3)), recursionDepth + 1);
	}

	if(definingInstruction.opcode() == spv::OpLoad && definingInstruction.wordCount() >= 4)
	{
		sw::Spirv::Object::ID storedValueId;
		if(tryResolveFunctionLocalStoredValue(shader, sw::Spirv::Object::ID(definingInstruction.word(3)), &storedValueId))
		{
			return valueResolvesToTextureSample(shader, storedValueId, recursionDepth + 1);
		}
	}

	return false;
}

bool locationZeroOutputIsTextureSamplePassthrough(const sw::SpirvShader &shader)
{
	uint32_t locationZeroOutputId = 0;
	if(!tryGetLocationZeroOutputId(shader, &locationZeroOutputId))
	{
		return false;
	}

	bool sawStore = false;
	sw::Spirv::Object::ID storedValueId;
	for(auto insn : shader)
	{
		if(insn.opcode() != spv::OpStore || insn.word(1) != locationZeroOutputId)
		{
			continue;
		}

		sw::Spirv::Object::ID candidateValueId(insn.word(2));
		if(sawStore)
		{
			if(storedValueId != candidateValueId)
			{
				return false;
			}
			continue;
		}

		storedValueId = candidateValueId;
		sawStore = true;
	}

	if(!sawStore)
	{
		return false;
	}

	return valueResolvesToTextureSample(shader, storedValueId);
}

uint32_t getNumInputComponentsAtLocation(const sw::SpirvShader &shader, int32_t location)
{
	if(location < 0)
	{
		return 0;
	}

	uint32_t componentCount = 0;
	const uint32_t firstComponent = static_cast<uint32_t>(location) << 2;
	for(; componentCount < 4; componentCount++)
	{
		if(shader.inputs[firstComponent | componentCount].Type == sw::Spirv::ATTRIBTYPE_UNUSED)
		{
			break;
		}
	}
	return componentCount;
}

struct TextureDescriptorRef
{
	uint32_t descriptorSet = 0;
	uint32_t binding = 0;
	uint32_t arrayElement = 0;
	bool hasNonConstantArrayElement = false;
	backend::GraphicsExecutableDescriptorBindingInfo bindingInfo = {};
};

void appendUniqueDescriptorObjectId(std::vector<sw::Spirv::Object::ID> *descriptorObjectIds,
                                    sw::Spirv::Object::ID objectId)
{
	if(descriptorObjectIds == nullptr)
	{
		return;
	}

	for(const auto existingId : *descriptorObjectIds)
	{
		if(existingId == objectId)
		{
			return;
		}
	}
	descriptorObjectIds->push_back(objectId);
}

bool tryGetDescriptorArrayElement(const sw::SpirvShader &shader,
                                  sw::Spirv::Object::ID objectId,
                                  uint32_t *arrayElement,
                                  bool *hasNonConstantArrayElement,
                                  int recursionDepth = 0)
{
	if(arrayElement == nullptr || hasNonConstantArrayElement == nullptr || recursionDepth > 8)
	{
		return false;
	}

	sw::SpirvShader::InsnIterator definingInstruction;
	if(!tryFindInstructionByResultId(shader, objectId, &definingInstruction))
	{
		return false;
	}

	switch(definingInstruction.opcode())
	{
	case spv::OpLoad:
		if(definingInstruction.wordCount() >= 4)
		{
			return tryGetDescriptorArrayElement(shader,
			                                   sw::Spirv::Object::ID(definingInstruction.word(3)),
			                                   arrayElement,
			                                   hasNonConstantArrayElement,
			                                   recursionDepth + 1);
		}
		return false;
	case spv::OpCopyObject:
	case spv::OpCopyLogical:
		if(definingInstruction.wordCount() >= 4)
		{
			return tryGetDescriptorArrayElement(shader,
			                                   sw::Spirv::Object::ID(definingInstruction.word(3)),
			                                   arrayElement,
			                                   hasNonConstantArrayElement,
			                                   recursionDepth + 1);
		}
		return false;
	case spv::OpAccessChain:
	case spv::OpInBoundsAccessChain:
		if(definingInstruction.wordCount() >= 5)
		{
			auto indexObject = shader.getObject(sw::Spirv::Object::ID(definingInstruction.word(4)));
			if(indexObject.kind == sw::Spirv::Object::Kind::Constant && !indexObject.constantValue.empty())
			{
				*arrayElement = static_cast<uint32_t>(indexObject.constantValue[0]);
				return true;
			}

			*hasNonConstantArrayElement = true;
			return false;
		}
		return false;
	case spv::OpPtrAccessChain:
		// OpPtrAccessChain adds an extra "element" operand before indices. Handle the common case
		// where glslang uses that operand to index descriptor arrays.
		if(definingInstruction.wordCount() >= 5)
		{
			auto indexObject = shader.getObject(sw::Spirv::Object::ID(definingInstruction.word(4)));
			if(indexObject.kind == sw::Spirv::Object::Kind::Constant && !indexObject.constantValue.empty())
			{
				*arrayElement = static_cast<uint32_t>(indexObject.constantValue[0]);
				return true;
			}

			*hasNonConstantArrayElement = true;
			return false;
		}
		return false;
	default:
		return false;
	}
}

bool appendTextureDescriptorRef(const sw::SpirvShader &shader,
                                const backend::GraphicsExecutableCreateInfo &createInfo,
                                sw::Spirv::Object::ID objectId,
                                std::vector<TextureDescriptorRef> *refs)
{
	if(refs == nullptr || createInfo.queryDescriptorBindingInfo == nullptr)
	{
		return false;
	}

	const auto decorationIt = shader.descriptorDecorations.find(objectId);
	if(decorationIt == shader.descriptorDecorations.end())
	{
		return false;
	}

	const auto &decoration = decorationIt->second;
	if(decoration.DescriptorSet < 0 || decoration.Binding < 0)
	{
		return false;
	}

	const uint32_t descriptorSet = static_cast<uint32_t>(decoration.DescriptorSet);
	const uint32_t descriptorBinding = static_cast<uint32_t>(decoration.Binding);

	TextureDescriptorRef ref = {};
	ref.descriptorSet = descriptorSet;
	ref.binding = descriptorBinding;
	ref.arrayElement = 0;
	ref.hasNonConstantArrayElement = false;
	tryGetDescriptorArrayElement(shader, objectId, &ref.arrayElement, &ref.hasNonConstantArrayElement);
	if(!createInfo.queryDescriptorBindingInfo(createInfo.queryDescriptorBindingInfoUserdata,
	                                          descriptorSet,
	                                          descriptorBinding,
	                                          &ref.bindingInfo))
	{
		return false;
	}

	for(auto &existingRef : *refs)
	{
		if(existingRef.descriptorSet != descriptorSet || existingRef.binding != descriptorBinding)
		{
			continue;
		}

		if(existingRef.hasNonConstantArrayElement)
		{
			return true;
		}

		if(ref.hasNonConstantArrayElement)
		{
			existingRef.hasNonConstantArrayElement = true;
			existingRef.arrayElement = 0;
			existingRef.bindingInfo = ref.bindingInfo;
			return true;
		}

		if(existingRef.arrayElement == ref.arrayElement)
		{
			return true;
		}
	}
	refs->push_back(ref);
	return true;
}

bool tryCollectSampledDescriptorObjectIds(const sw::SpirvShader &shader,
                                         sw::Spirv::Object::ID sampledImageId,
                                         std::vector<sw::Spirv::Object::ID> *descriptorObjectIds,
                                         int recursionDepth = 0)
{
	if(descriptorObjectIds == nullptr || recursionDepth > 8)
	{
		return false;
	}

	sw::SpirvShader::InsnIterator definingInstruction;
	if(tryFindInstructionByResultId(shader, sampledImageId, &definingInstruction))
	{
		if(definingInstruction.opcode() == spv::OpSampledImage && definingInstruction.wordCount() >= 5)
		{
			std::vector<sw::Spirv::Object::ID> imageDescriptorObjectIds;
			if(!tryCollectSampledDescriptorObjectIds(shader,
			                                        sw::Spirv::Object::ID(definingInstruction.word(3)),
			                                        &imageDescriptorObjectIds,
			                                        recursionDepth + 1))
			{
				return false;
			}

			std::vector<sw::Spirv::Object::ID> samplerDescriptorObjectIds;
			if(!tryCollectSampledDescriptorObjectIds(shader,
			                                        sw::Spirv::Object::ID(definingInstruction.word(4)),
			                                        &samplerDescriptorObjectIds,
			                                        recursionDepth + 1))
			{
				return false;
			}

			for(const auto descriptorObjectId : imageDescriptorObjectIds)
			{
				appendUniqueDescriptorObjectId(descriptorObjectIds, descriptorObjectId);
			}
			for(const auto descriptorObjectId : samplerDescriptorObjectIds)
			{
				appendUniqueDescriptorObjectId(descriptorObjectIds, descriptorObjectId);
			}
			return !descriptorObjectIds->empty();
		}

		if(definingInstruction.opcode() == spv::OpImage && definingInstruction.wordCount() >= 4)
		{
			return tryCollectSampledDescriptorObjectIds(shader,
			                                           sw::Spirv::Object::ID(definingInstruction.word(3)),
			                                           descriptorObjectIds,
			                                           recursionDepth + 1);
		}

		if((definingInstruction.opcode() == spv::OpCopyObject || definingInstruction.opcode() == spv::OpCopyLogical) &&
		   definingInstruction.wordCount() >= 4)
		{
			return tryCollectSampledDescriptorObjectIds(shader,
			                                           sw::Spirv::Object::ID(definingInstruction.word(3)),
			                                           descriptorObjectIds,
			                                           recursionDepth + 1);
		}

		if(definingInstruction.opcode() == spv::OpLoad && definingInstruction.wordCount() >= 4)
		{
			sw::Spirv::Object::ID storedValueId;
			if(tryResolveFunctionLocalStoredValue(shader, sw::Spirv::Object::ID(definingInstruction.word(3)), &storedValueId))
			{
				return tryCollectSampledDescriptorObjectIds(shader,
				                                           storedValueId,
				                                           descriptorObjectIds,
				                                           recursionDepth + 1);
			}
		}
	}

	const auto decorationIt = shader.descriptorDecorations.find(sampledImageId);
	if(decorationIt == shader.descriptorDecorations.end() ||
	   decorationIt->second.DescriptorSet < 0 ||
	   decorationIt->second.Binding < 0)
	{
		return false;
	}

	appendUniqueDescriptorObjectId(descriptorObjectIds, sampledImageId);
	return true;
}

bool collectTextureDescriptorRefs(const sw::SpirvShader &shader,
                                  const backend::GraphicsExecutableCreateInfo &createInfo,
                                  std::vector<TextureDescriptorRef> *refs)
{
	if(refs == nullptr)
	{
		return false;
	}

	for(auto insn : shader)
	{
		if(!isBootstrapTextureSamplingOpcode(insn.opcode()) || insn.wordCount() < 4)
		{
			continue;
		}

		std::vector<sw::Spirv::Object::ID> descriptorObjectIds;
		if(!tryCollectSampledDescriptorObjectIds(shader,
		                                        sw::Spirv::Object::ID(insn.word(3)),
		                                        &descriptorObjectIds))
		{
			return false;
		}

		for(const auto descriptorObjectId : descriptorObjectIds)
		{
			if(!appendTextureDescriptorRef(shader, createInfo, descriptorObjectId, refs))
			{
				return false;
			}
		}
	}

	return !refs->empty();
}

bool collectStorageImageDescriptorRefs(const sw::SpirvShader &shader,
                                       const backend::GraphicsExecutableCreateInfo &createInfo,
                                       std::vector<TextureDescriptorRef> *refs)
{
	if(refs == nullptr)
	{
		return false;
	}

	for(auto insn : shader)
	{
		uint32_t imageId = 0;
		if(insn.opcode() == spv::OpImageRead && insn.wordCount() >= 5)
		{
			imageId = insn.word(3);
		}
		else if(insn.opcode() == spv::OpImageWrite && insn.wordCount() >= 4)
		{
			imageId = insn.word(1);
		}
		else
		{
			continue;
		}

		std::vector<sw::Spirv::Object::ID> descriptorObjectIds;
		if(!tryCollectSampledDescriptorObjectIds(shader,
		                                        sw::Spirv::Object::ID(imageId),
		                                        &descriptorObjectIds))
		{
			return false;
		}

		for(const auto descriptorObjectId : descriptorObjectIds)
		{
			if(!appendTextureDescriptorRef(shader, createInfo, descriptorObjectId, refs))
			{
				return false;
			}
		}
	}

	return !refs->empty();
}

[[maybe_unused]] bool tryBuildImageResourcePlan(const sw::SpirvShader &shader,
                                                const backend::GraphicsExecutableCreateInfo &createInfo,
                                                backend::GraphicsExecutableImageResourcePlan *plan)
{
	if(plan == nullptr)
	{
		return false;
	}

	backend::GraphicsExecutableImageResourcePlan resourcePlan = {};

	if(shaderContainsTextureSampling(shader))
	{
		std::vector<TextureDescriptorRef> sampledRefs;
		if(!collectTextureDescriptorRefs(shader, createInfo, &sampledRefs))
		{
			return false;
		}

		for(const auto &ref : sampledRefs)
		{
			backend::GraphicsExecutableDescriptorRef descriptorRef = {};
			descriptorRef.descriptorType = ref.bindingInfo.descriptorType;
			descriptorRef.descriptorCount = ref.bindingInfo.descriptorCount;
			descriptorRef.descriptorSet = ref.descriptorSet;
			descriptorRef.binding = ref.binding;
			descriptorRef.arrayElement = ref.arrayElement;
			descriptorRef.hasNonConstantArrayElement = ref.hasNonConstantArrayElement;
			descriptorRef.isDynamic = ref.bindingInfo.isDynamic;
			descriptorRef.dynamicOffsetIndex = ref.bindingInfo.dynamicOffsetIndex;
			resourcePlan.sampledDescriptors.push_back(descriptorRef);
		}
	}

	if(shaderContainsStorageImageReadWrite(shader))
	{
		std::vector<TextureDescriptorRef> storageRefs;
		if(!collectStorageImageDescriptorRefs(shader, createInfo, &storageRefs))
		{
			return false;
		}

		for(const auto &ref : storageRefs)
		{
			if(ref.bindingInfo.descriptorType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE &&
			   ref.bindingInfo.descriptorType != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
			{
				continue;
			}

			backend::GraphicsExecutableDescriptorRef descriptorRef = {};
			descriptorRef.descriptorType = ref.bindingInfo.descriptorType;
			descriptorRef.descriptorCount = ref.bindingInfo.descriptorCount;
			descriptorRef.descriptorSet = ref.descriptorSet;
			descriptorRef.binding = ref.binding;
			descriptorRef.arrayElement = ref.arrayElement;
			descriptorRef.hasNonConstantArrayElement = ref.hasNonConstantArrayElement;
			descriptorRef.isDynamic = ref.bindingInfo.isDynamic;
			descriptorRef.dynamicOffsetIndex = ref.bindingInfo.dynamicOffsetIndex;
			resourcePlan.storageDescriptors.push_back(descriptorRef);
		}
	}

	if(resourcePlan.sampledDescriptors.empty() && resourcePlan.storageDescriptors.empty())
	{
		return false;
	}

	*plan = std::move(resourcePlan);
	return true;
}

bool tryAppendDescriptorRef(const sw::SpirvShader &shader,
                            const backend::GraphicsExecutableCreateInfo &createInfo,
                            sw::Spirv::Object::ID objectId,
                            std::vector<backend::GraphicsExecutableDescriptorRef> *refs)
{
	if(refs == nullptr || createInfo.queryDescriptorBindingInfo == nullptr)
	{
		return false;
	}

	const auto decorationIt = shader.descriptorDecorations.find(objectId);
	if(decorationIt == shader.descriptorDecorations.end())
	{
		return false;
	}

	const auto &decoration = decorationIt->second;
	if(decoration.DescriptorSet < 0 || decoration.Binding < 0)
	{
		return false;
	}

	const uint32_t descriptorSet = static_cast<uint32_t>(decoration.DescriptorSet);
	const uint32_t descriptorBinding = static_cast<uint32_t>(decoration.Binding);

	uint32_t arrayElement = 0;
	bool hasNonConstantArrayElement = false;
	tryGetDescriptorArrayElement(shader, objectId, &arrayElement, &hasNonConstantArrayElement);

	backend::GraphicsExecutableDescriptorBindingInfo bindingInfo = {};
	if(!createInfo.queryDescriptorBindingInfo(createInfo.queryDescriptorBindingInfoUserdata,
	                                          descriptorSet,
	                                          descriptorBinding,
	                                          &bindingInfo))
	{
		return false;
	}

	backend::GraphicsExecutableDescriptorRef candidate = {};
	candidate.descriptorType = bindingInfo.descriptorType;
	candidate.descriptorCount = bindingInfo.descriptorCount;
	candidate.descriptorSet = descriptorSet;
	candidate.binding = descriptorBinding;
	candidate.arrayElement = arrayElement;
	candidate.hasNonConstantArrayElement = hasNonConstantArrayElement;
	candidate.isDynamic = bindingInfo.isDynamic;
	candidate.dynamicOffsetIndex = bindingInfo.dynamicOffsetIndex;

	for(auto &existing : *refs)
	{
		if(existing.descriptorSet != descriptorSet || existing.binding != descriptorBinding)
		{
			continue;
		}

		if(existing.hasNonConstantArrayElement)
		{
			return true;
		}

		if(candidate.hasNonConstantArrayElement)
		{
			existing.hasNonConstantArrayElement = true;
			existing.arrayElement = 0;
			existing.descriptorType = candidate.descriptorType;
			existing.descriptorCount = candidate.descriptorCount;
			existing.isDynamic = candidate.isDynamic;
			existing.dynamicOffsetIndex = candidate.dynamicOffsetIndex;
			return true;
		}

		if(existing.arrayElement == candidate.arrayElement)
		{
			return true;
		}
	}

	refs->push_back(candidate);
	return true;
}

[[maybe_unused]] bool tryBuildResourcePlan(const sw::SpirvShader &shader,
                                           const backend::GraphicsExecutableCreateInfo &createInfo,
                                           backend::GraphicsExecutableResourcePlan *plan)
{
	if(plan == nullptr)
	{
		return false;
	}

	backend::GraphicsExecutableResourcePlan resourcePlan = {};
	resourcePlan.descriptorSetCount = createInfo.descriptorSetCount;
	resourcePlan.dynamicOffsetCount = createInfo.dynamicOffsetCount;
	resourcePlan.pushConstantSize = createInfo.pushConstantSize;

	if(createInfo.queryDescriptorBindingInfo != nullptr)
	{
		for(const auto &entry : shader.descriptorDecorations)
		{
			if(entry.second.DescriptorSet < 0 || entry.second.Binding < 0)
			{
				continue;
			}
			if(!tryAppendDescriptorRef(shader, createInfo, entry.first, &resourcePlan.descriptors))
			{
				return false;
			}
		}
	}

	*plan = std::move(resourcePlan);
	return true;
}

	[[maybe_unused]] bool tryBuildTexturePlan(const sw::SpirvShader &shader,
	                                          const backend::GraphicsExecutableCreateInfo &createInfo,
	                                          backend::GraphicsExecutableTexturePlan *plan)
{
	if(plan == nullptr)
	{
		return false;
	}

	if(!shaderContainsTextureSampling(shader))
	{
		return false;
	}

	std::vector<TextureDescriptorRef> refs;
	if(!collectTextureDescriptorRefs(shader, createInfo, &refs))
	{
		return false;
	}

	backend::GraphicsExecutableTexturePlan texturePlan = {};
	const bool directSamplePassthrough = (getNumInputComponentsAtLocation(shader, 0) == 2) &&
	                                     locationZeroOutputIsTextureSamplePassthrough(shader);
	const bool hasStorageImageOps = shaderContainsStorageImageReadWrite(shader);

	if(refs.size() == 1)
	{
		const auto &ref = refs[0];
		texturePlan.imageDescriptorSet = ref.descriptorSet;
		texturePlan.imageBinding = ref.binding;
		texturePlan.imageArrayElement = ref.arrayElement;
		texturePlan.samplerDescriptorSet = ref.descriptorSet;
		texturePlan.samplerBinding = ref.binding;
		texturePlan.samplerArrayElement = ref.arrayElement;
		if(ref.bindingInfo.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		{
			texturePlan.resourceKind = backend::GraphicsExecutableTextureResourceKind::CombinedImageSampler;
			texturePlan.bootstrapSupported = directSamplePassthrough &&
			                                 !hasStorageImageOps &&
			                                 !ref.hasNonConstantArrayElement &&
			                                 ref.arrayElement < ref.bindingInfo.descriptorCount;
		}
		else
		{
			texturePlan.resourceKind = backend::GraphicsExecutableTextureResourceKind::Other;
		}
		*plan = texturePlan;
		return true;
	}

	if(refs.size() == 2)
	{
		const TextureDescriptorRef *sampledImageRef = nullptr;
		const TextureDescriptorRef *samplerRef = nullptr;
		for(const auto &ref : refs)
		{
			if(ref.bindingInfo.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && sampledImageRef == nullptr)
			{
				sampledImageRef = &ref;
			}
			else if(ref.bindingInfo.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER && samplerRef == nullptr)
			{
				samplerRef = &ref;
			}
		}

		if(sampledImageRef != nullptr && samplerRef != nullptr)
		{
			texturePlan.resourceKind = backend::GraphicsExecutableTextureResourceKind::SeparateImageSampler;
			texturePlan.imageDescriptorSet = sampledImageRef->descriptorSet;
			texturePlan.imageBinding = sampledImageRef->binding;
			texturePlan.imageArrayElement = sampledImageRef->arrayElement;
			texturePlan.samplerDescriptorSet = samplerRef->descriptorSet;
			texturePlan.samplerBinding = samplerRef->binding;
			texturePlan.samplerArrayElement = samplerRef->arrayElement;
			texturePlan.bootstrapSupported = directSamplePassthrough &&
			                                 !hasStorageImageOps &&
			                                 !sampledImageRef->hasNonConstantArrayElement &&
			                                 !samplerRef->hasNonConstantArrayElement &&
			                                 sampledImageRef->arrayElement < sampledImageRef->bindingInfo.descriptorCount &&
			                                 samplerRef->arrayElement < samplerRef->bindingInfo.descriptorCount;
			*plan = texturePlan;
			return true;
		}
	}

	texturePlan.resourceKind = backend::GraphicsExecutableTextureResourceKind::Other;
	texturePlan.imageDescriptorSet = refs[0].descriptorSet;
	texturePlan.imageBinding = refs[0].binding;
	texturePlan.imageArrayElement = refs[0].arrayElement;
	texturePlan.samplerDescriptorSet = refs[0].descriptorSet;
	texturePlan.samplerBinding = refs[0].binding;
	texturePlan.samplerArrayElement = refs[0].arrayElement;
	texturePlan.bootstrapSupported = false;
	*plan = texturePlan;
	return true;
}

bool tryBuildStaticBootstrapFragmentConfig(const sw::SpirvShader &shader, backend::FragmentBootstrapConfig *config)
{
	if(config == nullptr)
	{
		return false;
	}

	if(shader.hasBuiltinInput(spv::BuiltInFragCoord) && shader.getAnalysis().ContainsDiscard)
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::FragCoordDiscardLeftConstantColor;
		config->colorR = 1.0f;
		config->colorG = 0.0f;
		config->colorB = 0.0f;
		config->colorA = 1.0f;
		return true;
	}

	if(shader.hasBuiltinInput(spv::BuiltInFragCoord))
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::FragCoordQuadrants;
		return true;
	}

	if(shader.hasBuiltinInput(spv::BuiltInPointCoord))
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::PointCoordGradient;
		return true;
	}

	const uint32_t featureMask = fragmentFeatureMaskForShader(shader);
	if(shaderContainsTextureSampling(shader) &&
	   (featureMask & static_cast<uint32_t>(backend::GraphicsExecutableFragmentFeature::Derivatives)) != 0 &&
	   !shader.hasBuiltinInput(spv::BuiltInFragCoord) &&
	   !shader.hasBuiltinInput(spv::BuiltInPointCoord) &&
	   !shader.hasBuiltinInput(spv::BuiltInFrontFacing) &&
	   !shader.getAnalysis().ContainsDiscard &&
	   getNumInputComponentsAtLocation(shader, 0) >= 2 &&
	   getNumInputComponentsAtLocation(shader, 1) >= 3)
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::DerivativeLitTexture2DColor;
		return true;
	}

	const uint32_t inputComponents = getNumInputComponentsAtLocation(shader, 0);

	if(inputComponents >= 3 && shader.inputs[0].Flat)
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::FlatInterpolatedColor;
		return true;
	}

	if(inputComponents >= 3)
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::InterpolatedColorBlueNearFragDepth;
		config->nearDepth = 0.2f;
		config->farDepth = 0.8f;
		return true;
	}

	if(shader.hasBuiltinInput(spv::BuiltInFrontFacing))
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::FrontFacingBinaryColors;
		config->colorR = 1.0f;
		config->colorG = 0.0f;
		config->colorB = 0.0f;
		config->colorA = 1.0f;
		config->backColorR = 0.0f;
		config->backColorG = 0.0f;
		config->backColorB = 1.0f;
		config->backColorA = 1.0f;
		return true;
	}

	if(tryGetBootstrapFragmentConstantColor(shader, config))
	{
		return true;
	}

	if(inputComponents >= 3)
	{
		config->shaderKind = backend::FragmentBootstrapShaderKind::InterpolatedColor;
		return true;
	}

	return false;
}

}  // namespace

namespace backend {

std::shared_ptr<GraphicsExecutable> GraphicsExecutable::create(const GraphicsExecutableCreateInfo &createInfo)
{
	if(!createInfo.vertexModule || createInfo.vertexModule->stage() != VK_SHADER_STAGE_VERTEX_BIT)
	{
		return nullptr;
	}

	if(createInfo.fragmentModule && createInfo.fragmentModule->stage() != VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		return nullptr;
	}

	float bootstrapPointSize = 64.0f;
	if(createInfo.vertexShader)
	{
		tryGetBootstrapVertexPointSizeConstant(*createInfo.vertexShader, &bootstrapPointSize);
	}

	FragmentBootstrapConfig bootstrapFragmentConfig = {};
	const bool bootstrapFragmentConfigValid = createInfo.fragmentShader &&
	                                          tryBuildStaticBootstrapFragmentConfig(*createInfo.fragmentShader, &bootstrapFragmentConfig);
	GraphicsExecutableTexturePlan texturePlan = {};
	GraphicsExecutableImageResourcePlan imageResourcePlan = {};
	uint32_t fragmentFeatureMask = 0;
	GraphicsExecutableResourcePlan resourcePlan = {};
	bool resourcePlanValid = false;
	if(createInfo.fragmentShader)
	{
		ShaderAnalysisQueryContext queryContext = {};
		queryContext.query = createInfo.queryDescriptorBindingInfo;
		queryContext.userdata = createInfo.queryDescriptorBindingInfoUserdata;

		sw::ShaderCompilerAnalysisContext analysisContext = {};
		analysisContext.queryDescriptorBindingInfo = queryDescriptorBindingInfoForAnalysis;
		analysisContext.queryDescriptorBindingInfoUserdata = &queryContext;
		analysisContext.descriptorSetCount = createInfo.descriptorSetCount;
		analysisContext.dynamicOffsetCount = createInfo.dynamicOffsetCount;
		analysisContext.pushConstantSize = createInfo.pushConstantSize;

		const sw::ShaderCompilerAnalysisResult analysisResult =
		    sw::analyzeGraphicsFragmentShader(createInfo.fragmentModule ? createInfo.fragmentModule->entryPoint() : std::string("main"),
		                                     createInfo.fragmentShader->insns,
		                                     analysisContext);
		texturePlan = toBackendTexturePlan(analysisResult.texturePlan);
		imageResourcePlan = toBackendImageResourcePlan(analysisResult.imageResourcePlan);
		fragmentFeatureMask = toBackendFragmentFeatureMask(analysisResult.fragmentFeatureMask);
		resourcePlan = toBackendResourcePlan(analysisResult.resourcePlan);
		resourcePlanValid = true;
	}
	else
	{
		resourcePlan.descriptorSetCount = createInfo.descriptorSetCount;
		resourcePlan.dynamicOffsetCount = createInfo.dynamicOffsetCount;
		resourcePlan.pushConstantSize = createInfo.pushConstantSize;
		resourcePlanValid = true;
	}
	const bool texturePlanValid = (texturePlan.resourceKind != backend::GraphicsExecutableTextureResourceKind::None);
	const bool imageResourcePlanValid = !imageResourcePlan.sampledDescriptors.empty() || !imageResourcePlan.storageDescriptors.empty();

	if(texturePlanValid &&
	   bootstrapFragmentConfigValid &&
	   bootstrapFragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor)
	{
		texturePlan.bootstrapSupported = true;
	}

	uint32_t triangleBootstrapUnsupportedReasonMask = 0;
	if(!createInfo.fragmentShader)
	{
		triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::NoFragmentStage);
	}
	else
	{
		const uint32_t features = fragmentFeatureMask;
		if((features & static_cast<uint32_t>(GraphicsExecutableFragmentFeature::StorageImageReadWrite)) != 0)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::StorageImageReadWrite);
		}
		if((features & static_cast<uint32_t>(GraphicsExecutableFragmentFeature::ImageQueryOrFetch)) != 0)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::ImageQueryOrFetch);
		}
		const bool supportsDerivativeTextureTemplate =
		    bootstrapFragmentConfigValid &&
		    bootstrapFragmentConfig.shaderKind == FragmentBootstrapShaderKind::DerivativeLitTexture2DColor;
		if((features & static_cast<uint32_t>(GraphicsExecutableFragmentFeature::Derivatives)) != 0 &&
		   !supportsDerivativeTextureTemplate)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::Derivatives);
		}
		if((features & static_cast<uint32_t>(GraphicsExecutableFragmentFeature::Atomics)) != 0)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::Atomics);
		}
		if((features & static_cast<uint32_t>(GraphicsExecutableFragmentFeature::Subgroup)) != 0)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::Subgroup);
		}

		bool hasBufferDescriptors = false;
		bool hasNonConstantArrayElement = false;
		if(resourcePlanValid)
		{
			for(const auto &ref : resourcePlan.descriptors)
			{
				if(ref.hasNonConstantArrayElement)
				{
					hasNonConstantArrayElement = true;
				}
				if(isBufferDescriptorType(ref.descriptorType))
				{
					hasBufferDescriptors = true;
				}
			}
		}
		if(hasBufferDescriptors)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::BufferDescriptorsPresent);
		}
		if(hasNonConstantArrayElement)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::NonConstantDescriptorArrayElement);
		}

		const bool samplesTextures = texturePlanValid;
		if(samplesTextures)
		{
			if(!texturePlan.bootstrapSupported && !supportsDerivativeTextureTemplate)
			{
				triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::TextureSamplingUnsupported);
			}
		}
		else if(!bootstrapFragmentConfigValid)
		{
			triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::MissingBootstrapFragmentConfig);
		}

		const bool containsDiscard = (features & static_cast<uint32_t>(GraphicsExecutableFragmentFeature::Discard)) != 0;
		if(containsDiscard)
		{
			const bool supportedDiscardTemplate = !samplesTextures &&
			                                     bootstrapFragmentConfigValid &&
			                                     bootstrapFragmentConfig.shaderKind == FragmentBootstrapShaderKind::FragCoordDiscardLeftConstantColor;
			if(!supportedDiscardTemplate)
			{
				triangleBootstrapUnsupportedReasonMask |= static_cast<uint32_t>(GraphicsExecutableTriangleBootstrapUnsupportedReason::DiscardUnsupported);
			}
		}
	}

	return std::shared_ptr<GraphicsExecutable>(new GraphicsExecutable(createInfo.vertexModule->entryPoint(),
	                                                                  createInfo.fragmentModule ? createInfo.fragmentModule->entryPoint() : std::string(),
	                                                                  createInfo.vertexModule->vertexLowering(),
	                                                                  bootstrapPointSize,
	                                                                  bootstrapFragmentConfigValid,
	                                                                  std::move(bootstrapFragmentConfig),
	                                                                  texturePlanValid,
	                                                                  std::move(texturePlan),
	                                                                  imageResourcePlanValid,
	                                                                  std::move(imageResourcePlan),
	                                                                  resourcePlanValid,
	                                                                  std::move(resourcePlan),
	                                                                  fragmentFeatureMask,
	                                                                  triangleBootstrapUnsupportedReasonMask));
}

std::shared_ptr<GraphicsExecutable> GraphicsExecutable::create(const std::shared_ptr<sw::SemanticIRModule> &vertexModule,
                                                               const std::shared_ptr<sw::SemanticIRModule> &fragmentModule)
{
	GraphicsExecutableCreateInfo createInfo = {};
	createInfo.vertexModule = vertexModule;
	createInfo.fragmentModule = fragmentModule;
	return create(createInfo);
}

}  // namespace backend
