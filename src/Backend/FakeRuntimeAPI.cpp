#include "FakeRuntimeAPI.hpp"

namespace backend {

ModuleHandle FakeRuntimeAPI::createModule(const std::string &sourceOrIR)
{
	(void)sourceOrIR;
	return ModuleHandle{ nextId++ };
}

}  // namespace backend
