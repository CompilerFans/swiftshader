add_library(vk_backend EXCLUDE_FROM_ALL
    BackendConfig.hpp
)

target_include_directories(vk_backend
    PUBLIC
        ".."
        "${SWIFTSHADER_DIR}/include"
)

target_link_libraries(vk_backend
    PUBLIC
        vk_base
        vk_system
)
