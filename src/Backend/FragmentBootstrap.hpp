#ifndef SWIFTSHADER_FRAGMENT_BOOTSTRAP_HPP_
#define SWIFTSHADER_FRAGMENT_BOOTSTRAP_HPP_

#include "RuntimeAPI.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace backend {

struct FragmentBootstrapInvocation
{
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t exportMask = 1;
	uint32_t helperInvocation = 0;
};

struct FragmentBootstrapConfig
{
	float colorR = 1.0f;
	float colorG = 0.0f;
	float colorB = 0.0f;
	float colorA = 1.0f;
};

std::string fragmentBootstrapCudaSource(const FragmentBootstrapConfig &config = {});
bool runFragmentBootstrap(RuntimeAPI &runtime, uint32_t width, uint32_t height, const std::vector<FragmentBootstrapInvocation> &invocations, const FragmentBootstrapConfig &config, std::vector<uint8_t> *colorBuffer);
void launchFragmentBootstrap(RuntimeAPI &runtime);

}  // namespace backend

#endif  // SWIFTSHADER_FRAGMENT_BOOTSTRAP_HPP_
