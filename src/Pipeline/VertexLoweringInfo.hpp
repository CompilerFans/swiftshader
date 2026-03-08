#ifndef SWIFTSHADER_VERTEX_LOWERING_INFO_HPP_
#define SWIFTSHADER_VERTEX_LOWERING_INFO_HPP_

#include <cstdint>

namespace sw {

struct VertexLoweringInfo
{
	bool usesPositionAttribute = false;
	uint32_t positionAttributeLocation = 0;
	uint32_t positionBinding = 0;
	uint32_t positionInputComponentCount = 0;
	uint32_t vertexStride = 0;
	uint32_t positionOffset = 0;
	bool usesVertexIndex = false;
	bool usesInstanceIndex = false;
	float constantOffsetX = 0.0f;
	float constantOffsetY = 0.0f;
	float constantOffsetZ = 0.0f;
};

}  // namespace sw

#endif  // SWIFTSHADER_VERTEX_LOWERING_INFO_HPP_
