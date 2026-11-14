# Add source to this project's executable.
add_library(
        EphemeralRenderer_Internal STATIC
        VkInitializer.cpp
        VkImages.cpp
        VkDescriptors.cpp
        VkPipelines.cpp
        VkEngine.cpp
        VkLoader.cpp
        VMA.cpp
)

target_sources(
        EphemeralRenderer_Internal
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        VkEngine.h

        PRIVATE
        VkDescriptors.h
        VkImages.h
        VkInitializer.h
        VkLoader.h
        VkPipelines.h
        VkTypes.h
)


set_property(TARGET EphemeralRenderer_Internal PROPERTY CXX_STANDARD 20)
target_include_directories(EphemeralRenderer_Internal PRIVATE "${EPHEMERAL_ROOT_DIR}")

target_link_libraries(EphemeralRenderer_Internal PRIVATE
        Vulkan::Vulkan
        fmt::fmt
        std::stb
        SDL3::SDL3
        vk-bootstrap
        DearImGui::DearImGui
        fastgltf::fastgltf
        Falcon::Falcon
        VulkanMemoryAllocator::Headers
        EphemeralUtils_Internal
)

target_precompile_headers(
        EphemeralRenderer_Internal
        PUBLIC
        <optional>
        <vector>
        <memory>
        <string>
        <vector>
        <unordered_map>
        #        <vulkan/vulkan.h> # Don't include this; can break MSVC.
)