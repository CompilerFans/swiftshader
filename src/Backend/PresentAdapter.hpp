#ifndef SWIFTSHADER_PRESENT_ADAPTER_HPP_
#define SWIFTSHADER_PRESENT_ADAPTER_HPP_

#include "Backend/ResourceStateTracker.hpp"

#include <cstdint>
#include <memory>

namespace backend {

struct PresentAdapterCapture
{
	uint32_t acquireCount = 0;
	uint32_t presentCount = 0;
	uint64_t lastAcquireImageId = 0;
	uint64_t lastPresentImageId = 0;
};

class PresentAdapter
{
public:
	virtual ~PresentAdapter() = default;

	virtual bool isFallbackAdapter() const = 0;
	virtual void acquire(ResourceStateTracker &tracker, uint64_t imageId) = 0;
	virtual void present(ResourceStateTracker &tracker, uint64_t imageId) = 0;
};

std::unique_ptr<PresentAdapter> createFallbackPresentAdapter();
std::unique_ptr<PresentAdapter> createPresentAdapter();

void resetPresentAdapterCapture();
const PresentAdapterCapture &lastPresentAdapterCapture();

}  // namespace backend

#endif  // SWIFTSHADER_PRESENT_ADAPTER_HPP_
