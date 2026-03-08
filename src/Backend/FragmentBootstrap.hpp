#ifndef SWIFTSHADER_FRAGMENT_BOOTSTRAP_HPP_
#define SWIFTSHADER_FRAGMENT_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace backend {

enum class FragmentBootstrapShaderKind
{
	ConstantColor,
	FragCoordQuadrants,
	InterpolatedColor,
};

struct FragmentBootstrapInvocation
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t exportMask = 1;
	uint32_t helperInvocation = 0;
	float barycentric0 = 0.0f;
	float barycentric1 = 0.0f;
	float barycentric2 = 0.0f;
};

struct FragmentBootstrapConfig
{
	FragmentBootstrapShaderKind shaderKind = FragmentBootstrapShaderKind::ConstantColor;
	float colorR = 1.0f;
	float colorG = 0.0f;
	float colorB = 0.0f;
	float colorA = 1.0f;
	float vertexColor0R = 1.0f;
	float vertexColor0G = 1.0f;
	float vertexColor0B = 1.0f;
	float vertexColor0A = 1.0f;
	float vertexColor1R = 1.0f;
	float vertexColor1G = 1.0f;
	float vertexColor1B = 1.0f;
	float vertexColor1A = 1.0f;
	float vertexColor2R = 1.0f;
	float vertexColor2G = 1.0f;
	float vertexColor2B = 1.0f;
	float vertexColor2A = 1.0f;
};

std::string fragmentBootstrapCudaSource(const FragmentBootstrapConfig &config = {});
bool runFragmentBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, const std::vector<FragmentBootstrapInvocation> &invocations, const FragmentBootstrapConfig &config, std::vector<uint8_t> *colorBuffer);
void launchFragmentBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_FRAGMENT_BOOTSTRAP_HPP_
