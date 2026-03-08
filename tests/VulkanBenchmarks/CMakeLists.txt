# Copyright 2020 The SwiftShader Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(ROOT_PROJECT_COMPILE_OPTIONS
)

set(ROOT_PROJECT_LINK_LIBRARIES
    ${OS_LIBS}
    ${SWIFTSHADER_LIBS}
)

set(VULKAN_BENCHMARKS_SRC_FILES
    ClearImageBenchmarks.cpp
    ComputeBenchmarks.cpp
    main.cpp
    TriangleBenchmarks.cpp
)

add_executable(VulkanBenchmarks
    ${VULKAN_BENCHMARKS_SRC_FILES}
)

add_executable(draw-fps-observer
    DrawFpsObserver.cpp
)

if (NOT TARGET benchmark::benchmark)
    message(FATAL_ERROR "Missing required target: benchmark::benchmark")
endif()

if (NOT TARGET VulkanWrapper)
    message(FATAL_ERROR "Missing required target: VulkanWrapper")
endif()

add_dependencies(VulkanBenchmarks
    vk_swiftshader
)

add_dependencies(draw-fps-observer
    vk_swiftshader
)

set_target_properties(VulkanBenchmarks PROPERTIES
    FOLDER "Tests/Benchmarks"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set_target_properties(draw-fps-observer PROPERTIES
    FOLDER "Tests/Benchmarks"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(VulkanBenchmarks
    PRIVATE
        "${SWIFTSHADER_DIR}/include/"
)

target_include_directories(draw-fps-observer
    PRIVATE
        "${SWIFTSHADER_DIR}/include/"
)

target_compile_options(VulkanBenchmarks
    PRIVATE
        ${ROOT_PROJECT_COMPILE_OPTIONS}
)

target_compile_options(draw-fps-observer
    PRIVATE
        ${ROOT_PROJECT_COMPILE_OPTIONS}
)

target_link_options(VulkanBenchmarks
    PRIVATE
        ${SWIFTSHADER_LINK_FLAGS}
)

target_link_options(draw-fps-observer
    PRIVATE
        ${SWIFTSHADER_LINK_FLAGS}
)

target_link_libraries(VulkanBenchmarks
    PRIVATE
        benchmark::benchmark
        VulkanWrapper
        ${ROOT_PROJECT_LINK_LIBRARIES}
)

target_link_libraries(draw-fps-observer
    PRIVATE
        VulkanWrapper
        ${ROOT_PROJECT_LINK_LIBRARIES}
)
