#include "SemanticIRBuilder.hpp"

#include <spirv/unified1/spirv.hpp>

#include <unordered_map>
#include <unordered_set>

namespace sw {

namespace {

struct MinimalTypeInfo
{
	spv::Op opcode = spv::OpNop;
	uint32_t elementTypeId = 0;
	uint32_t componentCount = 0;
	spv::StorageClass storageClass = spv::StorageClassMax;
};

struct MinimalDecorationInfo
{
	bool hasLocation = false;
	uint32_t location = 0;
	bool hasBuiltin = false;
	spv::BuiltIn builtin = static_cast<spv::BuiltIn>(-1);
};

struct MinimalVariableInfo
{
	uint32_t typeId = 0;
};

uint32_t stringSizeInWords(const uint32_t *words, uint32_t wordCount, uint32_t startWord)
{
	for(uint32_t index = startWord; index < wordCount; index++)
	{
		const char *stringWord = reinterpret_cast<const char *>(&words[index]);
		for(uint32_t byteIndex = 0; byteIndex < 4; byteIndex++)
		{
			if(stringWord[byteIndex] == '\0')
			{
				return 1 + index - startWord;
			}
		}
	}
	return 0;
}

bool entryPointMatches(VkShaderStageFlagBits stage, const std::string &entryPointName, const uint32_t *words, uint32_t wordCount)
{
	if(wordCount < 3)
	{
		return false;
	}

	spv::ExecutionModel executionModel = static_cast<spv::ExecutionModel>(words[1]);
	VkShaderStageFlagBits instructionStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	switch(executionModel)
	{
	case spv::ExecutionModelVertex: instructionStage = VK_SHADER_STAGE_VERTEX_BIT; break;
	case spv::ExecutionModelFragment: instructionStage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
	case spv::ExecutionModelGLCompute: instructionStage = VK_SHADER_STAGE_COMPUTE_BIT; break;
	default: break;
	}

	if(instructionStage != stage)
	{
		return false;
	}

	return entryPointName == reinterpret_cast<const char *>(&words[3]);
}

ParsedSpirvInfo parseMinimalVertexLowering(VkShaderStageFlagBits stage, const std::string &entryPointName, const SpirvBinary &spirv)
{
	ParsedSpirvInfo parsed = { stage, entryPointName };
	if(stage != VK_SHADER_STAGE_VERTEX_BIT || spirv.size() < 5)
	{
		return parsed;
	}

	std::unordered_set<uint32_t> interfaceIds;
	std::unordered_map<uint32_t, MinimalDecorationInfo> decorations;
	std::unordered_map<uint32_t, MinimalTypeInfo> types;
	std::unordered_map<uint32_t, MinimalVariableInfo> variables;

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

		switch(opcode)
		{
		case spv::OpEntryPoint:
			if(entryPointMatches(stage, entryPointName, words, wordCount))
			{
				uint32_t interfaceOffset = 3 + stringSizeInWords(words, wordCount, 3);
				for(uint32_t index = interfaceOffset; index < wordCount; index++)
				{
					interfaceIds.emplace(words[index]);
				}
			}
			break;
		case spv::OpDecorate:
			if(wordCount >= 3)
			{
				auto &decoration = decorations[words[1]];
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
				default:
					break;
				}
			}
			break;
		case spv::OpTypeFloat:
		case spv::OpTypeInt:
			if(wordCount >= 2)
			{
				types[words[1]].opcode = opcode;
				types[words[1]].componentCount = 1;
			}
			break;
		case spv::OpTypeVector:
			if(wordCount >= 4)
			{
				auto &type = types[words[1]];
				type.opcode = opcode;
				type.elementTypeId = words[2];
				type.componentCount = words[3];
			}
			break;
		case spv::OpTypePointer:
			if(wordCount >= 4)
			{
				auto &type = types[words[1]];
				type.opcode = opcode;
				type.storageClass = static_cast<spv::StorageClass>(words[2]);
				type.elementTypeId = words[3];
			}
			break;
		case spv::OpVariable:
			if(wordCount >= 4)
			{
				variables[words[2]].typeId = words[1];
			}
			break;
		default:
			break;
		}

		instruction += wordCount;
	}

	for(uint32_t interfaceId : interfaceIds)
	{
		auto decorationIt = decorations.find(interfaceId);
		auto variableIt = variables.find(interfaceId);
		if(variableIt == variables.end())
		{
			continue;
		}

		auto pointerTypeIt = types.find(variableIt->second.typeId);
		if(pointerTypeIt == types.end())
		{
			continue;
		}

		const auto &pointerType = pointerTypeIt->second;
		if(pointerType.storageClass != spv::StorageClassInput)
		{
			continue;
		}

		if(decorationIt != decorations.end() && decorationIt->second.hasBuiltin)
		{
			switch(decorationIt->second.builtin)
			{
			case spv::BuiltInVertexIndex:
				parsed.vertexLowering.usesVertexIndex = true;
				break;
			case spv::BuiltInInstanceIndex:
				parsed.vertexLowering.usesInstanceIndex = true;
				break;
			default:
				break;
			}
			continue;
		}

		if(decorationIt == decorations.end() || !decorationIt->second.hasLocation)
		{
			continue;
		}

		auto valueTypeIt = types.find(pointerType.elementTypeId);
		if(valueTypeIt == types.end())
		{
			continue;
		}

		const auto &valueType = valueTypeIt->second;
		if(valueType.opcode == spv::OpTypeVector && valueType.componentCount >= 3 && decorationIt->second.location == 0)
		{
			parsed.vertexLowering.usesPositionAttribute = true;
			parsed.vertexLowering.positionAttributeLocation = decorationIt->second.location;
		}
	}

	return parsed;
}

}  // namespace

std::shared_ptr<SemanticIRModule> SemanticIRBuilder::build(VkShaderStageFlagBits stage, const std::string &entryPointName, const SpirvBinary &spirv) const
{
	return build(parseMinimalVertexLowering(stage, entryPointName, spirv));
}

}  // namespace sw
