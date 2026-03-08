set(BACKEND_SRC_FILES
    BackendConfig.hpp
    BackendFactory.cpp
    ComputeExecutable.cpp
    ComputeExecutable.hpp
    FakeRuntimeAPI.cpp
    FakeRuntimeAPI.hpp
    BackendFactory.hpp
    ExecutionBackend.cpp
    ResourceStateTracker.cpp
    ResourceStateTracker.hpp
    RuntimeAPI.cpp
    RuntimeAPI.hpp
    ExecutionBackend.hpp
)

add_library(vk_backend EXCLUDE_FROM_ALL
    ${BACKEND_SRC_FILES}
)

target_include_directories(vk_backend
    PUBLIC
        ".."
        "${SWIFTSHADER_DIR}/include"
        "${SWIFTSHADER_DIR}/third_party/SPIRV-Headers/include"
        "${SWIFTSHADER_DIR}/third_party/SPIRV-Tools/include"
        "${SWIFTSHADER_DIR}/third_party/marl/include"
)

target_compile_options(vk_backend
    PRIVATE
        ${SWIFTSHADER_COMPILE_OPTIONS}
        ${WARNINGS_AS_ERRORS}
)

target_link_libraries(vk_backend
    PUBLIC
        vk_base
        vk_system
)
