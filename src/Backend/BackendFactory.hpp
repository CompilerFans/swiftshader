#ifndef SWIFTSHADER_BACKEND_FACTORY_HPP_
#define SWIFTSHADER_BACKEND_FACTORY_HPP_

#include "ExecutionBackend.hpp"
#include "RuntimeAPI.hpp"

namespace backend {

BackendKind defaultBackendKind();
std::unique_ptr<RuntimeAPI> createRuntimeAPI();

}  // namespace backend

#endif  // SWIFTSHADER_BACKEND_FACTORY_HPP_
