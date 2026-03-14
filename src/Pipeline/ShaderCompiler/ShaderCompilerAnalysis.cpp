#include "ShaderCompilerAnalysis.hpp"

#include "spirv-tools/libspirv.hpp"
#include <spirv/unified1/spirv.hpp>

#include <cstring>
#include <unordered_map>
#include <vector>

namespace sw {
namespace {

struct DecorationInfo
{
	int descriptorSet = -1;
	int binding = -1;
	bool hasLocation = false;
	uint32_t location = 0;
	bool hasBuiltin = false;
	spv::BuiltIn builtin = static_cast<spv::BuiltIn>(-1);
	bool flat = false;
};

struct InstructionInfo
{
	spv::Op opcode = spv::OpNop;
	std::vector<uint32_t> words;
};

struct TypeInfo
{
	spv::Op opcode = spv::OpNop;
	uint32_t elementTypeId = 0;
	uint32_t componentCount = 0;
	spv::StorageClass storageClass = spv::StorageClassMax;
};

struct DescriptorUse
{
	uint32_t objectId = 0;
	uint32_t arrayElement = 0;
	bool hasNonConstantArrayElement = false;
};

bool isTextureSamplingOpcode(spv::Op opcode)
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

bool instructionHasTrackedResult(spv::Op opcode, uint32_t wordCount)
{
	switch(opcode)
	{
	case spv::OpConstant:
	case spv::OpConstantComposite:
	case spv::OpVariable:
	case spv::OpLoad:
	case spv::OpCopyObject:
	case spv::OpCopyLogical:
	case spv::OpSampledImage:
	case spv::OpImage:
	case spv::OpImageSampleImplicitLod:
	case spv::OpImageSampleExplicitLod:
	case spv::OpImageSampleProjImplicitLod:
	case spv::OpImageSampleProjExplicitLod:
	case spv::OpAccessChain:
	case spv::OpInBoundsAccessChain:
	case spv::OpPtrAccessChain:
		return wordCount >= 3;
	default:
		return false;
	}
}

void collectInstructions(const SpirvBinary &spirv,
                         std::unordered_map<uint32_t, DecorationInfo> *decorations,
                         std::unordered_map<uint32_t, InstructionInfo> *definitions,
                         std::unordered_map<uint32_t, TypeInfo> *types)
{
	if(spirv.size() < 5)
	{
		return;
	}

	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		const uint32_t *words = &spirv[instruction];
		if(opcode == spv::OpDecorate && wordCount >= 3)
		{
			auto &decoration = (*decorations)[words[1]];
			switch(static_cast<spv::Decoration>(words[2]))
			{
			case spv::DecorationLocation:
				decoration.hasLocation = true;
				decoration.location = (wordCount > 3) ? words[3] : 0;
				break;
			case spv::DecorationBuiltIn:
				decoration.hasBuiltin = true;
				decoration.builtin = static_cast<spv::BuiltIn>((wordCount > 3) ? words[3] : 0);
				break;
			case spv::DecorationFlat:
				decoration.flat = true;
				break;
			case spv::DecorationDescriptorSet:
				decoration.descriptorSet = (wordCount > 3) ? static_cast<int>(words[3]) : 0;
				break;
			case spv::DecorationBinding:
				decoration.binding = (wordCount > 3) ? static_cast<int>(words[3]) : 0;
				break;
			default:
				break;
			}
		}

		if(instructionHasTrackedResult(opcode, wordCount))
		{
			InstructionInfo info = {};
			info.opcode = opcode;
			info.words.assign(words, words + wordCount);
			(*definitions)[words[2]] = std::move(info);
		}

		switch(opcode)
		{
		case spv::OpTypeFloat:
		case spv::OpTypeInt:
			if(wordCount >= 2)
			{
				(*types)[words[1]].opcode = opcode;
				(*types)[words[1]].componentCount = 1;
			}
			break;
		case spv::OpTypeVector:
			if(wordCount >= 4)
			{
				auto &type = (*types)[words[1]];
				type.opcode = opcode;
				type.elementTypeId = words[2];
				type.componentCount = words[3];
			}
			break;
		case spv::OpTypePointer:
			if(wordCount >= 4)
			{
				auto &type = (*types)[words[1]];
				type.opcode = opcode;
				type.storageClass = static_cast<spv::StorageClass>(words[2]);
				type.elementTypeId = words[3];
			}
			break;
		default:
			break;
		}

		instruction += wordCount;
	}
}

uint32_t computeFragmentFeatureMask(const SpirvBinary &spirv)
{
	uint32_t mask = 0;

	if(spirv.size() < 5)
	{
		return mask;
	}

	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		switch(opcode)
		{
		case spv::OpKill:
		case spv::OpDemoteToHelperInvocation:
		case spv::OpTerminateInvocation:
			mask |= static_cast<uint32_t>(ShaderFragmentFeature::Discard);
			break;
		case spv::OpImageRead:
		case spv::OpImageWrite:
			mask |= static_cast<uint32_t>(ShaderFragmentFeature::StorageImageReadWrite);
			break;
		case spv::OpImageFetch:
		case spv::OpImageQuerySize:
		case spv::OpImageQuerySizeLod:
		case spv::OpImageQueryLod:
		case spv::OpImageQueryLevels:
		case spv::OpImageQuerySamples:
			mask |= static_cast<uint32_t>(ShaderFragmentFeature::ImageQueryOrFetch);
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
			mask |= static_cast<uint32_t>(ShaderFragmentFeature::Derivatives);
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
			mask |= static_cast<uint32_t>(ShaderFragmentFeature::Atomics);
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
			mask |= static_cast<uint32_t>(ShaderFragmentFeature::Subgroup);
			break;
		default:
			break;
		}

		instruction += wordCount;
	}

	return mask;
}

bool resolveDescriptorUses(uint32_t objectId,
                           const SpirvBinary &spirv,
                           const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                           const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                           std::vector<DescriptorUse> *descriptorUses,
                           int recursionDepth = 0)
{
	if(descriptorUses == nullptr || recursionDepth > 8)
	{
		return false;
	}

	auto decorationIt = decorations.find(objectId);
	if(decorationIt != decorations.end() &&
	   decorationIt->second.descriptorSet >= 0 &&
	   decorationIt->second.binding >= 0)
	{
		for(const auto &existingUse : *descriptorUses)
		{
			if(existingUse.objectId == objectId)
			{
				return true;
			}
		}
		DescriptorUse use = {};
		use.objectId = objectId;
		descriptorUses->push_back(use);
		return true;
	}

	auto definitionIt = definitions.find(objectId);
	if(definitionIt == definitions.end())
	{
		return false;
	}

	const auto &definition = definitionIt->second;
	switch(definition.opcode)
	{
	case spv::OpLoad:
	case spv::OpCopyObject:
	case spv::OpCopyLogical:
	case spv::OpImage:
		return definition.words.size() >= 4 &&
		       resolveDescriptorUses(definition.words[3], spirv, decorations, definitions, descriptorUses, recursionDepth + 1);
	case spv::OpSampledImage:
		if(definition.words.size() < 5)
		{
			return false;
		}
		{
			const bool resolvedImage = resolveDescriptorUses(definition.words[3], spirv, decorations, definitions, descriptorUses, recursionDepth + 1);
			const bool resolvedSampler = resolveDescriptorUses(definition.words[4], spirv, decorations, definitions, descriptorUses, recursionDepth + 1);
			return resolvedImage || resolvedSampler;
		}
	case spv::OpAccessChain:
	case spv::OpInBoundsAccessChain:
	case spv::OpPtrAccessChain:
		if(definition.words.size() < 4)
		{
			return false;
		}
		{
			std::vector<DescriptorUse> baseUses;
			const bool resolvedBase = resolveDescriptorUses(definition.words[3], spirv, decorations, definitions, &baseUses, recursionDepth + 1);
			if(!resolvedBase)
			{
				return false;
			}

			bool resolvedArrayElement = false;
			uint32_t arrayElement = 0;
			bool hasNonConstantArrayElement = false;
			const size_t indexWord = 4;
			if(definition.words.size() > indexWord)
			{
				uint32_t indexId = definition.words[indexWord];
				auto constantIt = definitions.find(indexId);
				if(constantIt == definitions.end())
				{
					hasNonConstantArrayElement = true;
				}
				else
				{
					const auto &constantDef = constantIt->second;
					if(constantDef.opcode == spv::OpConstant && constantDef.words.size() >= 4)
					{
						arrayElement = constantDef.words[3];
						resolvedArrayElement = true;
					}
					else
					{
						hasNonConstantArrayElement = true;
					}
				}
			}

			for(auto &baseUse : baseUses)
			{
				if(resolvedArrayElement)
				{
					baseUse.arrayElement = arrayElement;
				}
				baseUse.hasNonConstantArrayElement = baseUse.hasNonConstantArrayElement || hasNonConstantArrayElement;
				descriptorUses->push_back(baseUse);
			}
			return true;
		}
	default:
		return false;
	}
}

bool appendDescriptorRef(const DescriptorUse &descriptorUse,
                         const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                         const ShaderCompilerAnalysisContext &context,
                         std::vector<ShaderDescriptorRef> *refs)
{
	if(refs == nullptr || context.queryDescriptorBindingInfo == nullptr)
	{
		return false;
	}

	const auto decorationIt = decorations.find(descriptorUse.objectId);
	if(decorationIt == decorations.end() ||
	   decorationIt->second.descriptorSet < 0 ||
	   decorationIt->second.binding < 0)
	{
		return false;
	}

	ShaderDescriptorBindingInfo bindingInfo = {};
	const uint32_t descriptorSet = static_cast<uint32_t>(decorationIt->second.descriptorSet);
	const uint32_t binding = static_cast<uint32_t>(decorationIt->second.binding);
	if(!context.queryDescriptorBindingInfo(context.queryDescriptorBindingInfoUserdata,
	                                       descriptorSet,
	                                       binding,
	                                       &bindingInfo))
	{
		return false;
	}

	for(const auto &existing : *refs)
	{
		if(existing.descriptorSet == descriptorSet && existing.binding == binding)
		{
			return true;
		}
	}

	ShaderDescriptorRef ref = {};
	ref.descriptorType = bindingInfo.descriptorType;
	ref.descriptorCount = bindingInfo.descriptorCount;
	ref.descriptorSet = descriptorSet;
	ref.binding = binding;
	ref.arrayElement = descriptorUse.arrayElement;
	ref.hasNonConstantArrayElement = descriptorUse.hasNonConstantArrayElement;
	ref.isDynamic = bindingInfo.isDynamic;
	ref.dynamicOffsetIndex = bindingInfo.dynamicOffsetIndex;
	refs->push_back(ref);
	return true;
}

std::vector<ShaderDescriptorRef> collectSampledDescriptorRefs(const SpirvBinary &spirv,
                                                              const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                                              const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                                              const ShaderCompilerAnalysisContext &context)
{
	std::vector<ShaderDescriptorRef> refs;

	if(spirv.size() < 5)
	{
		return refs;
	}

	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);
		const uint32_t *words = &spirv[instruction];

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		if(isTextureSamplingOpcode(opcode) && wordCount >= 4)
		{
			std::vector<DescriptorUse> descriptorUses;
			resolveDescriptorUses(words[3], spirv, decorations, definitions, &descriptorUses);
			for(const auto &descriptorUse : descriptorUses)
			{
				appendDescriptorRef(descriptorUse, decorations, context, &refs);
			}
		}

		instruction += wordCount;
	}

	return refs;
}

std::vector<ShaderDescriptorRef> collectStorageDescriptorRefs(const SpirvBinary &spirv,
                                                              const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                                              const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                                              const ShaderCompilerAnalysisContext &context)
{
	std::vector<ShaderDescriptorRef> refs;

	if(spirv.size() < 5)
	{
		return refs;
	}

	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);
		const uint32_t *words = &spirv[instruction];

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		uint32_t imageId = 0;
		if(opcode == spv::OpImageRead && wordCount >= 4)
		{
			imageId = words[3];
		}
		else if(opcode == spv::OpImageWrite && wordCount >= 2)
		{
			imageId = words[1];
		}

		if(imageId != 0)
		{
			std::vector<DescriptorUse> descriptorUses;
			resolveDescriptorUses(imageId, spirv, decorations, definitions, &descriptorUses);
			for(const auto &descriptorUse : descriptorUses)
			{
				appendDescriptorRef(descriptorUse, decorations, context, &refs);
			}
		}

		instruction += wordCount;
	}

	return refs;
}

uint32_t getInputComponentCountAtLocation(const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                          const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                          const std::unordered_map<uint32_t, TypeInfo> &types,
                                          uint32_t location)
{
	for(const auto &[objectId, decoration] : decorations)
	{
		if(!decoration.hasLocation || decoration.location != location)
		{
			continue;
		}

		const auto definitionIt = definitions.find(objectId);
		if(definitionIt == definitions.end() || definitionIt->second.opcode != spv::OpVariable || definitionIt->second.words.size() < 4)
		{
			continue;
		}

		uint32_t pointerTypeId = definitionIt->second.words[1];
		const auto pointerTypeIt = types.find(pointerTypeId);
		if(pointerTypeIt == types.end() || pointerTypeIt->second.storageClass != spv::StorageClassInput)
		{
			continue;
		}

		const auto valueTypeIt = types.find(pointerTypeIt->second.elementTypeId);
		if(valueTypeIt == types.end())
		{
			continue;
		}

		if(valueTypeIt->second.opcode == spv::OpTypeVector)
		{
			return valueTypeIt->second.componentCount;
		}
		return valueTypeIt->second.componentCount;
	}

	return 0;
}

bool tryGetLocationZeroOutputId(const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                const std::unordered_map<uint32_t, TypeInfo> &types,
                                uint32_t *outputId)
{
	if(outputId == nullptr)
	{
		return false;
	}

	uint32_t foundId = 0;
	for(const auto &[objectId, decoration] : decorations)
	{
		if(!decoration.hasLocation || decoration.location != 0)
		{
			continue;
		}

		const auto definitionIt = definitions.find(objectId);
		if(definitionIt == definitions.end() || definitionIt->second.opcode != spv::OpVariable || definitionIt->second.words.size() < 4)
		{
			continue;
		}

		const auto pointerTypeIt = types.find(definitionIt->second.words[1]);
		if(pointerTypeIt == types.end() || pointerTypeIt->second.storageClass != spv::StorageClassOutput)
		{
			continue;
		}

		if(foundId != 0)
		{
			return false;
		}
		foundId = objectId;
	}

	if(foundId == 0)
	{
		return false;
	}

	*outputId = foundId;
	return true;
}

bool tryResolveFunctionLocalStoredValue(const SpirvBinary &spirv,
                                        const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                        const std::unordered_map<uint32_t, TypeInfo> &types,
                                        uint32_t pointerId,
                                        uint32_t *storedValueId)
{
	if(storedValueId == nullptr)
	{
		return false;
	}

	const auto definitionIt = definitions.find(pointerId);
	if(definitionIt == definitions.end() || definitionIt->second.opcode != spv::OpVariable || definitionIt->second.words.size() < 4)
	{
		return false;
	}

	const auto pointerTypeIt = types.find(definitionIt->second.words[1]);
	if(pointerTypeIt == types.end() || pointerTypeIt->second.storageClass != spv::StorageClassFunction)
	{
		return false;
	}

	bool sawStore = false;
	uint32_t resolvedValueId = 0;
	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);
		const uint32_t *words = &spirv[instruction];

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		if(opcode == spv::OpStore && wordCount >= 3 && words[1] == pointerId)
		{
			if(sawStore && resolvedValueId != words[2])
			{
				return false;
			}
			resolvedValueId = words[2];
			sawStore = true;
		}

		instruction += wordCount;
	}

	if(!sawStore)
	{
		return false;
	}

	*storedValueId = resolvedValueId;
	return true;
}

bool valueResolvesToTextureSample(const SpirvBinary &spirv,
                                  const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                  const std::unordered_map<uint32_t, TypeInfo> &types,
                                  uint32_t valueId,
                                  int recursionDepth = 0)
{
	if(recursionDepth > 8)
	{
		return false;
	}

	const auto definitionIt = definitions.find(valueId);
	if(definitionIt == definitions.end())
	{
		return false;
	}

	const auto &definition = definitionIt->second;
	if(isTextureSamplingOpcode(definition.opcode))
	{
		return true;
	}
	if((definition.opcode == spv::OpCopyObject || definition.opcode == spv::OpCopyLogical) && definition.words.size() >= 4)
	{
		return valueResolvesToTextureSample(spirv, definitions, types, definition.words[3], recursionDepth + 1);
	}
	if(definition.opcode == spv::OpLoad && definition.words.size() >= 4)
	{
		uint32_t storedValueId = 0;
		if(tryResolveFunctionLocalStoredValue(spirv, definitions, types, definition.words[3], &storedValueId))
		{
			return valueResolvesToTextureSample(spirv, definitions, types, storedValueId, recursionDepth + 1);
		}
	}
	return false;
}

bool locationZeroOutputIsTextureSamplePassthrough(const SpirvBinary &spirv,
                                                  const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                                  const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                                  const std::unordered_map<uint32_t, TypeInfo> &types)
{
	uint32_t outputId = 0;
	if(!tryGetLocationZeroOutputId(decorations, definitions, types, &outputId))
	{
		return false;
	}

	bool sawStore = false;
	uint32_t storedValueId = 0;
	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);
		const uint32_t *words = &spirv[instruction];

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		if(opcode == spv::OpStore && wordCount >= 3 && words[1] == outputId)
		{
			if(sawStore && storedValueId != words[2])
			{
				return false;
			}
			storedValueId = words[2];
			sawStore = true;
		}

		instruction += wordCount;
	}

	if(!sawStore)
	{
		return false;
	}

	return valueResolvesToTextureSample(spirv, definitions, types, storedValueId);
}

bool tryBuildConstantColorFragmentInfo(const SpirvBinary &spirv,
                                       const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                       const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                       const std::unordered_map<uint32_t, TypeInfo> &types,
                                       ShaderCompilerAnalysisResult *result)
{
	if(result == nullptr)
	{
		return false;
	}

	uint32_t outputId = 0;
	if(!tryGetLocationZeroOutputId(decorations, definitions, types, &outputId))
	{
		return false;
	}

	uint32_t storedValueId = 0;
	bool sawStore = false;
	for(size_t instruction = 5; instruction < spirv.size();)
	{
		uint32_t instructionWord = spirv[instruction];
		uint32_t wordCount = instructionWord >> spv::WordCountShift;
		spv::Op opcode = static_cast<spv::Op>(instructionWord & spv::OpCodeMask);
		const uint32_t *words = &spirv[instruction];

		if(wordCount == 0 || instruction + wordCount > spirv.size())
		{
			break;
		}

		if(opcode == spv::OpStore && wordCount >= 3 && words[1] == outputId)
		{
			if(sawStore && storedValueId != words[2])
			{
				return false;
			}
			storedValueId = words[2];
			sawStore = true;
		}

		instruction += wordCount;
	}

	if(!sawStore)
	{
		return false;
	}

	const auto valueIt = definitions.find(storedValueId);
	if(valueIt == definitions.end() || valueIt->second.opcode != spv::OpConstantComposite || valueIt->second.words.size() < 7)
	{
		return false;
	}

	const auto &composite = valueIt->second.words;
	const auto component0 = definitions.find(composite[3]);
	const auto component1 = definitions.find(composite[4]);
	const auto component2 = definitions.find(composite[5]);
	const auto component3 = definitions.find(composite[6]);
	if(component0 == definitions.end() || component1 == definitions.end() ||
	   component2 == definitions.end() || component3 == definitions.end())
	{
		return false;
	}

	const auto isFloatConstant = [](const InstructionInfo &info) {
		return info.opcode == spv::OpConstant && info.words.size() >= 4;
	};
	if(!isFloatConstant(component0->second) || !isFloatConstant(component1->second) ||
	   !isFloatConstant(component2->second) || !isFloatConstant(component3->second))
	{
		return false;
	}

	auto toFloat = [](uint32_t word) {
		float value;
		std::memcpy(&value, &word, sizeof(float));
		return value;
	};

	result->staticFragmentKind = ShaderStaticFragmentKind::ConstantColor;
	result->colorR = toFloat(component0->second.words[3]);
	result->colorG = toFloat(component1->second.words[3]);
	result->colorB = toFloat(component2->second.words[3]);
	result->colorA = toFloat(component3->second.words[3]);
	return true;
}

bool hasBuiltinInput(const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                     const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                     const std::unordered_map<uint32_t, TypeInfo> &types,
                     spv::BuiltIn builtin)
{
	for(const auto &[objectId, decoration] : decorations)
	{
		if(!decoration.hasBuiltin || decoration.builtin != builtin)
		{
			continue;
		}

		const auto definitionIt = definitions.find(objectId);
		if(definitionIt == definitions.end() || definitionIt->second.opcode != spv::OpVariable || definitionIt->second.words.size() < 4)
		{
			continue;
		}

		const auto pointerTypeIt = types.find(definitionIt->second.words[1]);
		if(pointerTypeIt != types.end() && pointerTypeIt->second.storageClass == spv::StorageClassInput)
		{
			return true;
		}
	}
	return false;
}

bool hasBuiltinOutput(const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                      const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                      const std::unordered_map<uint32_t, TypeInfo> &types,
                      spv::BuiltIn builtin)
{
	(void)definitions;
	(void)types;
	for(const auto &[objectId, decoration] : decorations)
	{
		(void)objectId;
		if(!decoration.hasBuiltin || decoration.builtin != builtin)
		{
			continue;
		}
		return true;
	}
	return false;
}

bool isInputFlatAtLocation(const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                           const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                           const std::unordered_map<uint32_t, TypeInfo> &types,
                           uint32_t location)
{
	for(const auto &[objectId, decoration] : decorations)
	{
		if(!decoration.hasLocation || decoration.location != location)
		{
			continue;
		}

		const auto definitionIt = definitions.find(objectId);
		if(definitionIt == definitions.end() || definitionIt->second.opcode != spv::OpVariable || definitionIt->second.words.size() < 4)
		{
			continue;
		}

		const auto pointerTypeIt = types.find(definitionIt->second.words[1]);
		if(pointerTypeIt == types.end() || pointerTypeIt->second.storageClass != spv::StorageClassInput)
		{
			continue;
		}

		return decoration.flat;
	}
	return false;
}

ShaderTexturePlan buildTexturePlan(const SpirvBinary &spirv,
                                   const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                   const std::unordered_map<uint32_t, InstructionInfo> &definitions,
                                   const std::unordered_map<uint32_t, TypeInfo> &types,
                                   uint32_t fragmentFeatureMask,
                                   const std::vector<ShaderDescriptorRef> &refs)
{
	ShaderTexturePlan plan = {};
	if(refs.empty())
	{
		return plan;
	}

	const bool directSamplePassthrough =
	    (getInputComponentCountAtLocation(decorations, definitions, types, 0) == 2) &&
	    locationZeroOutputIsTextureSamplePassthrough(spirv, decorations, definitions, types);
	const bool hasStorageImageOps =
	    (fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::StorageImageReadWrite)) != 0;

	if(refs.size() == 1 && refs[0].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		plan.resourceKind = ShaderTextureResourceKind::CombinedImageSampler;
		plan.imageDescriptorSet = refs[0].descriptorSet;
		plan.imageBinding = refs[0].binding;
		plan.imageArrayElement = refs[0].arrayElement;
		plan.samplerDescriptorSet = refs[0].descriptorSet;
		plan.samplerBinding = refs[0].binding;
		plan.samplerArrayElement = refs[0].arrayElement;
		plan.bootstrapSupported = directSamplePassthrough &&
		                          !hasStorageImageOps &&
		                          !refs[0].hasNonConstantArrayElement &&
		                          refs[0].arrayElement < refs[0].descriptorCount;
		return plan;
	}

	if(refs.size() == 2)
	{
		const ShaderDescriptorRef *sampledImage = nullptr;
		const ShaderDescriptorRef *sampler = nullptr;
		for(const auto &ref : refs)
		{
			if(ref.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && sampledImage == nullptr)
			{
				sampledImage = &ref;
			}
			else if(ref.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER && sampler == nullptr)
			{
				sampler = &ref;
			}
		}

		if(sampledImage && sampler)
		{
			plan.resourceKind = ShaderTextureResourceKind::SeparateImageSampler;
			plan.imageDescriptorSet = sampledImage->descriptorSet;
			plan.imageBinding = sampledImage->binding;
			plan.imageArrayElement = sampledImage->arrayElement;
			plan.samplerDescriptorSet = sampler->descriptorSet;
			plan.samplerBinding = sampler->binding;
			plan.samplerArrayElement = sampler->arrayElement;
			plan.bootstrapSupported = directSamplePassthrough &&
			                          !hasStorageImageOps &&
			                          !sampledImage->hasNonConstantArrayElement &&
			                          !sampler->hasNonConstantArrayElement &&
			                          sampledImage->arrayElement < sampledImage->descriptorCount &&
			                          sampler->arrayElement < sampler->descriptorCount;
			return plan;
		}
	}

	plan.resourceKind = ShaderTextureResourceKind::Other;
	plan.imageDescriptorSet = refs[0].descriptorSet;
	plan.imageBinding = refs[0].binding;
	plan.imageArrayElement = refs[0].arrayElement;
	plan.samplerDescriptorSet = refs[0].descriptorSet;
	plan.samplerBinding = refs[0].binding;
	plan.samplerArrayElement = refs[0].arrayElement;
	return plan;
}

uint32_t buildUnsupportedReasonMask(uint32_t fragmentFeatureMask, const ShaderTexturePlan &texturePlan)
{
	uint32_t mask = 0;

	if((fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::StorageImageReadWrite)) != 0)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::StorageImageReadWrite);
	}
	if((fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::ImageQueryOrFetch)) != 0)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::ImageQueryOrFetch);
	}
	if((fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::Derivatives)) != 0)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::Derivatives);
	}
	if((fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::Atomics)) != 0)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::Atomics);
	}
	if((fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::Subgroup)) != 0)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::Subgroup);
	}
	if((fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::Discard)) != 0)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::DiscardUnsupported);
	}
	if(texturePlan.resourceKind != ShaderTextureResourceKind::None && !texturePlan.bootstrapSupported)
	{
		mask |= static_cast<uint32_t>(ShaderUnsupportedReason::TextureSamplingUnsupported);
	}

	return mask;
}

bool isBufferDescriptorType(VkDescriptorType descriptorType)
{
	switch(descriptorType)
	{
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
	case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return true;
	default:
		return false;
	}
}

std::vector<ShaderDescriptorRef> collectAllDescriptorRefs(const std::unordered_map<uint32_t, DecorationInfo> &decorations,
                                                          const ShaderCompilerAnalysisContext &context)
{
	std::vector<ShaderDescriptorRef> refs;
	for(const auto &[objectId, decoration] : decorations)
	{
		if(decoration.descriptorSet < 0 || decoration.binding < 0)
		{
			continue;
		}

		DescriptorUse use = {};
		use.objectId = objectId;
		appendDescriptorRef(use, decorations, context, &refs);
	}
	return refs;
}

}  // namespace

ShaderCompilerAnalysisResult analyzeGraphicsFragmentShader(const std::string &entryPointName,
                                                          const SpirvBinary &spirv,
                                                          const ShaderCompilerAnalysisContext &context)
{
	(void)entryPointName;

	std::unordered_map<uint32_t, DecorationInfo> decorations;
	std::unordered_map<uint32_t, InstructionInfo> definitions;
	std::unordered_map<uint32_t, TypeInfo> types;
	collectInstructions(spirv, &decorations, &definitions, &types);

	ShaderCompilerAnalysisResult result = {};
	result.fragmentFeatureMask = computeFragmentFeatureMask(spirv);

	const auto sampledRefs = collectSampledDescriptorRefs(spirv, decorations, definitions, context);
	result.texturePlan = buildTexturePlan(spirv, decorations, definitions, types, result.fragmentFeatureMask, sampledRefs);
	result.imageResourcePlan.sampledDescriptors = sampledRefs;

	const auto storageRefs = collectStorageDescriptorRefs(spirv, decorations, definitions, context);
	result.imageResourcePlan.storageDescriptors = storageRefs;

	result.resourcePlan.descriptorSetCount = context.descriptorSetCount;
	result.resourcePlan.dynamicOffsetCount = context.dynamicOffsetCount;
	result.resourcePlan.pushConstantSize = context.pushConstantSize;
	result.resourcePlan.descriptors = collectAllDescriptorRefs(decorations, context);

	result.unsupportedReasonMask = buildUnsupportedReasonMask(result.fragmentFeatureMask, result.texturePlan);
	if(result.texturePlan.resourceKind == ShaderTextureResourceKind::None)
	{
		if(hasBuiltinInput(decorations, definitions, types, spv::BuiltInFragCoord) &&
		   (result.fragmentFeatureMask & static_cast<uint32_t>(ShaderFragmentFeature::Discard)) != 0)
		{
			result.staticFragmentKind = ShaderStaticFragmentKind::FragCoordDiscardLeftConstantColor;
		}
		else if(!tryBuildConstantColorFragmentInfo(spirv, decorations, definitions, types, &result))
		{
			if(hasBuiltinInput(decorations, definitions, types, spv::BuiltInFragCoord))
			{
				result.staticFragmentKind = ShaderStaticFragmentKind::FragCoordQuadrants;
			}
			else if(hasBuiltinInput(decorations, definitions, types, spv::BuiltInPointCoord))
			{
				result.staticFragmentKind = ShaderStaticFragmentKind::PointCoordGradient;
			}
			else if(getInputComponentCountAtLocation(decorations, definitions, types, 0) >= 3 &&
			        isInputFlatAtLocation(decorations, definitions, types, 0))
			{
				result.staticFragmentKind = ShaderStaticFragmentKind::FlatInterpolatedColor;
			}
			else if(hasBuiltinOutput(decorations, definitions, types, spv::BuiltInFragDepth))
			{
				result.staticFragmentKind = ShaderStaticFragmentKind::InterpolatedColorBlueNearFragDepth;
			}
			else if(hasBuiltinInput(decorations, definitions, types, spv::BuiltInFrontFacing))
			{
				result.staticFragmentKind = ShaderStaticFragmentKind::FrontFacingBinaryColors;
			}
			else if(getInputComponentCountAtLocation(decorations, definitions, types, 0) >= 3)
			{
				result.staticFragmentKind = ShaderStaticFragmentKind::InterpolatedColor;
			}
		}
	}
	for(const auto &ref : result.resourcePlan.descriptors)
	{
		if(isBufferDescriptorType(ref.descriptorType))
		{
			result.unsupportedReasonMask |= static_cast<uint32_t>(ShaderUnsupportedReason::BufferDescriptorsPresent);
		}
		if(ref.hasNonConstantArrayElement)
		{
			result.unsupportedReasonMask |= static_cast<uint32_t>(ShaderUnsupportedReason::NonConstantDescriptorArrayElement);
		}
	}
	return result;
}

ShaderCompilerAnalysisResult analyzeGraphicsFragmentShader(const ShaderModuleInput &input,
                                                          const ShaderCompilerAnalysisContext &context)
{
	switch(input.kind())
	{
	case ShaderModuleInput::Kind::SpirvBinary:
		return analyzeGraphicsFragmentShader(input.entryPoint(), input.spirvBinary(), context);
	case ShaderModuleInput::Kind::SpirvAssemblyText:
		return analyzeGraphicsFragmentShaderAssembly(input.entryPoint(), input.spirvAssembly(), context);
	}

	return {};
}

ShaderCompilerAnalysisResult analyzeGraphicsFragmentShaderAssembly(const std::string &entryPointName,
                                                                  const std::string &spirvAssembly,
                                                                  const ShaderCompilerAnalysisContext &context)
{
	spvtools::SpirvTools core(SPV_ENV_VULKAN_1_0);
	std::vector<uint32_t> words;
	if(!core.Assemble(spirvAssembly, &words))
	{
		return {};
	}
	if(!core.Validate(words))
	{
		return {};
	}

	const SpirvBinary binary(words.data(), static_cast<uint32_t>(words.size()));
	return analyzeGraphicsFragmentShader(entryPointName, binary, context);
}

}  // namespace sw
