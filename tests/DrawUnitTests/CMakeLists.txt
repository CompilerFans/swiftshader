set(ROOT_PROJECT_COMPILE_OPTIONS
)

set(ROOT_PROJECT_LINK_LIBRARIES
    ${OS_LIBS}
    ${SWIFTSHADER_LIBS}
)

set(DRAW_UNIT_TESTS_SRC_FILES
    ${SWIFTSHADER_DIR}/tests/VulkanUnitTests/DrawTests.cpp
    ${SWIFTSHADER_DIR}/tests/VulkanUnitTests/main.cpp
)

add_executable(draw-unittests
    ${DRAW_UNIT_TESTS_SRC_FILES}
)

add_dependencies(draw-unittests
    vk_swiftshader
)

set_target_properties(draw-unittests PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(draw-unittests
    PRIVATE
        "${SWIFTSHADER_DIR}/include"
        "${SWIFTSHADER_DIR}/src"
        "${SWIFTSHADER_DIR}/tests/VulkanWrapper"
)

target_compile_definitions(draw-unittests
    PRIVATE
        "STANDALONE"
)

target_compile_options(draw-unittests
    PRIVATE
        ${ROOT_PROJECT_COMPILE_OPTIONS}
)

target_link_options(draw-unittests
    PRIVATE
        ${SWIFTSHADER_LINK_FLAGS}
)

target_link_libraries(draw-unittests
    PRIVATE
        vk_backend
        gtest
        gmock
        VulkanWrapper
        ${ROOT_PROJECT_LINK_LIBRARIES}
)
