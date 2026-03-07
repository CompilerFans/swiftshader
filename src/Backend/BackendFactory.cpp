#include "BackendFactory.hpp"

namespace backend {

BackendKind defaultBackendKind()
{
	return BackendKind::CPU;
}

}  // namespace backend
