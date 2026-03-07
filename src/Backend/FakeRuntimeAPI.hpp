#ifndef SWIFTSHADER_FAKE_RUNTIME_API_HPP_
#define SWIFTSHADER_FAKE_RUNTIME_API_HPP_

#include "RuntimeAPI.hpp"

namespace backend {

class FakeRuntimeAPI : public RuntimeAPI
{
public:
	ModuleHandle createModule(const std::string &sourceOrIR) override;

private:
	uint64_t nextId = 1;
};

}  // namespace backend

#endif  // SWIFTSHADER_FAKE_RUNTIME_API_HPP_
