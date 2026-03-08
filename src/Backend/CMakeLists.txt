set(BACKEND_SRC_FILES
    BackendConfig.hpp
    BackendFactory.cpp
    ComputeExecutable.cpp
    CustomExecutionBackend.cpp
    CpuExecutionBackend.cpp
    FragmentBootstrap.cpp
    GraphicsBootstrap.cpp
    RasterBootstrap.cpp
    ComputeExecutable.hpp
    FragmentBootstrap.hpp
    GraphicsBackend.cpp
    GraphicsBootstrap.hpp
    RasterBootstrap.hpp
    PresentAdapter.cpp
    PresentAdapter.hpp
    GraphicsBackend.hpp
    FakeRuntimeAPI.cpp
    FakeRuntimeAPI.hpp
    BackendFactory.hpp
    ExecutionBackend.cpp
    ExecutionBackendFactory.cpp
    ResourceStateTracker.cpp
    ResourceStateTracker.hpp
    RuntimeAPI.cpp
    RuntimeAPI.hpp
    ExecutionBackend.hpp
)

if(SWIFTSHADER_CUSTOM_GPU_USE_CUDA)
    list(APPEND BACKEND_SRC_FILES
        CudaCompilerDriver.cpp
        CudaCompilerDriver.hpp
        CudaRuntimeAPI.cpp
        CudaRuntimeAPI.hpp
    )
endif()

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

if(SWIFTSHADER_CUSTOM_GPU_USE_CUDA)
    target_include_directories(vk_backend
        PRIVATE
            ${CUDAToolkit_INCLUDE_DIRS}
    )
endif()

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
