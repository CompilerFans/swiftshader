set(BACKEND_SRC_FILES
    BackendConfig.hpp
    BackendFactory.cpp
    FakeRuntimeAPI.cpp
    FakeRuntimeAPI.hpp
    BackendFactory.hpp
    ExecutionBackend.cpp
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
