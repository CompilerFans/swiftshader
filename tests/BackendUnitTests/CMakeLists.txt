set(ROOT_PROJECT_COMPILE_OPTIONS
    ${SWIFTSHADER_COMPILE_OPTIONS}
)

set(ROOT_PROJECT_LINK_LIBRARIES
    ${OS_LIBS}
    ${SWIFTSHADER_LIBS}
)

set(BACKEND_UNIT_TESTS_SRC_FILES
    GraphicsBootstrapModeTests.cpp
    PresentAdapterFactoryTests.cpp
    ResourceStateTrackerTests.cpp
    ComputeDispatchValidationTests.cpp
    RuntimeAPITests.cpp
    CodegenEmitterTests.cpp
    AbiParityTests.cpp
    SpirvToSemanticIRTests.cpp
    ${SWIFTSHADER_DIR}/src/Pipeline/CudaLikeSourceEmitter.cpp
    ${SWIFTSHADER_DIR}/src/Pipeline/LlvmIREmitter.cpp
    ${SWIFTSHADER_DIR}/src/Backend/ComputeExecutable.cpp
    KernelIRTests.cpp
    SemanticIRTests.cpp
    BackendFactoryTests.cpp
    main.cpp
)

add_executable(backend-unittests
    ${BACKEND_UNIT_TESTS_SRC_FILES}
)

set_target_properties(backend-unittests PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(backend-unittests
    PRIVATE
        "${SWIFTSHADER_DIR}/include"
        "${SWIFTSHADER_DIR}/src"
        "${SWIFTSHADER_DIR}/third_party/marl/include"
        "${SWIFTSHADER_DIR}/third_party/SPIRV-Headers/include"
        "${SWIFTSHADER_DIR}/third_party/SPIRV-Tools/include"
)

target_compile_options(backend-unittests
    PRIVATE
        ${ROOT_PROJECT_COMPILE_OPTIONS}
)

target_link_options(backend-unittests
    PRIVATE
        ${SWIFTSHADER_LINK_FLAGS}
)

target_link_libraries(backend-unittests
    PRIVATE
        vk_backend
        gtest
        gmock
        ${ROOT_PROJECT_LINK_LIBRARIES}
)
