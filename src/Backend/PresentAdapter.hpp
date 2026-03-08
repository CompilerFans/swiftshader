#ifndef SWIFTSHADER_PRESENT_ADAPTER_HPP_
#define SWIFTSHADER_PRESENT_ADAPTER_HPP_

#include "Backend/ResourceStateTracker.hpp"

#include <memory>

namespace backend {

class PresentAdapter
{
public:
	virtual ~PresentAdapter() = default;

	virtual bool isFallbackAdapter() const = 0;
	virtual void acquire(ResourceStateTracker &tracker, uint64_t imageId) = 0;
	virtual void present(ResourceStateTracker &tracker, uint64_t imageId) = 0;
};

std::unique_ptr<PresentAdapter> createFallbackPresentAdapter();

}  // namespace backend

#endif  // SWIFTSHADER_PRESENT_ADAPTER_HPP_
