
# Add source to this project's executable.
add_library(
        EphemeralRenderer STATIC
        VkInitializers.cpp
        VkImages.cpp
        VkDescriptors.cpp
        VkPipelines.cpp
        VkEngine.cpp
        VkLoader.cpp
)

target_sources(
        EphemeralRenderer
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${EPHEMERAL_ROOT_DIR}/vulkan
        FILES
        VkEngine.h

        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${EPHEMERAL_ROOT_DIR}/vulkan
        FILES
        VkDescriptors.h
        VkImages.h
        VkInitializer.h
        VkLoader.h
        VkPipelines.h
        VkTypes.h
)


set_property(TARGET EphemeralRenderer PROPERTY CXX_STANDARD 20)
target_include_directories(EphemeralRenderer PUBLIC "${EPHEMERAL_ROOT_DIR}")

target_link_libraries(EphemeralRenderer PUBLIC
        vma
        Vulkan-Headers
#        Vulkan::Vulkan
        fmt::fmt
        std::stb
        SDL3::SDL3
        vk-bootstrap
        DearImGui::DearImGui
        fastgltf::fastgltf
        Falcon::Falcon
)

target_precompile_headers(
        EphemeralRenderer
        PUBLIC
        <optional>
        <vector>
        <memory>
        <string>
        <vector>
        <unordered_map>
        <vulkan/vulkan.h>
)

if (WIN32)
    add_custom_command(TARGET EphemeralRenderer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:EphemeralRenderer> $<TARGET_FILE_DIR:EphemeralRenderer>
            COMMAND_EXPAND_LISTS
    )
endif ()