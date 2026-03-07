set(ROOT_PROJECT_COMPILE_OPTIONS
    ${SWIFTSHADER_COMPILE_OPTIONS}
)

set(ROOT_PROJECT_LINK_LIBRARIES
    ${OS_LIBS}
    ${SWIFTSHADER_LIBS}
)

set(BACKEND_UNIT_TESTS_SRC_FILES
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
