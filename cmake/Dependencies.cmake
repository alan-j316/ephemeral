include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static library" FORCE)

#-------------------------------------------------------------------------
# Fetch SDL3
# ------------------------------------------------------------------------
message(STATUS "Downloading and including SDL3")
FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG bd7c9e467bed6ec5103925fc1fb0a9e3703d6e54
        SYSTEM
)

#-------------------------------------------------------------------------
# Fetch Falcon
# ------------------------------------------------------------------------
message(STATUS "Downloading and including Falcon")
FetchContent_Declare(
        Falcon
        GIT_REPOSITORY https://github.com/alan-j316/falcon-game-math.git
        GIT_TAG 31f71c916be699922e462ccf2f144ca0a9d27148
        SYSTEM
)

# https://stackoverflow.com/questions/69144529/vulkan-hpp-with-cmake
# Declare required Vulkan version
set(VULKAN_VER_MAJOR 1)
set(VULKAN_VER_MINOR 4)
set(VULKAN_VER_PATCH 358)
set(VULKAN_VERSION ${VULKAN_VER_MAJOR}.${VULKAN_VER_MINOR}.${VULKAN_VER_PATCH})

#-------------------------------------------------------------------------
# Fetch Vulkan C Headers
# ------------------------------------------------------------------------
message(STATUS "Downloading and including Vulkan C Headers")
FetchContent_Declare(
        VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_TAG v${VULKAN_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
set(PROJECT_IS_TOP_LEVEL OFF)


#-------------------------------------------------------------------------
# Fetch Vulkan Memory Allocator(VMA) Headers
#-------------------------------------------------------------------------
message(STATUS "Downloading and including VMA")
FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.5.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

#------------------------------------------------------------------------
# Fetch STB Headers
#------------------------------------------------------------------------
message(STATUS "Downloading and including STB")
FetchContent_Declare(
        STB
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

#-------------------------------------------------------------------------
# Fetch FastGLTF Headers
#-------------------------------------------------------------------------
message(STATUS "Downloading and including Vulkan C FastGLTF")
FetchContent_Declare(
        FastGLTF
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG v1.0.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)


#-------------------------------------------------------------------------
# Fetch IMGUI
#-------------------------------------------------------------------------
message(STATUS "Downloading and including Dear ImGui")
FetchContent_Declare(
        DearImGui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        SYSTEM
)

#-------------------------------------------------------------------------
# Fetch VKBootstrap
#-------------------------------------------------------------------------
message(STATUS "Downloading and including VkBootstrap")
FetchContent_Declare(
        VkBootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
        GIT_TAG v1.4.367
        SYSTEM
)

#-------------------------------------------------------------------------
# Fetch Volk
#-------------------------------------------------------------------------
message(STATUS "Downloading and including Volk")
FetchContent_Declare(
        Volk
        GIT_REPOSITORY https://github.com/zeux/volk.git
        GIT_TAG b8f91c03139cbfc8bc3803d4c40d099c33f9d3f7
        SYSTEM
)


#-------------------------------------------------------------------------
# Fetch FMT
#-------------------------------------------------------------------------
message(STATUS "Downloading and including fmt(Formatter)")
FetchContent_Declare(
        FMT
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 4229eb5820af49bf797641168624c8a754a1a4eb
        SYSTEM
)


FetchContent_MakeAvailable(SDL3)
#FetchContent_MakeAvailable(VulkanHeaders) # vulkanheaders has a proper CMakeLists
FetchContent_MakeAvailable(Falcon)
FetchContent_MakeAvailable(VulkanMemoryAllocator)
FetchContent_MakeAvailable(VkBootstrap)
FetchContent_MakeAvailable(FastGLTF)
FetchContent_MakeAvailable(FMT)
FetchContent_MakeAvailable(Volk)
FetchContent_MakeAvailable(STB)
FetchContent_MakeAvailable(DearImGui)

# NOTE: Source directory will always be in the lowercase of fetch-content declared name
# Since ImGui doesn't have cmake support we need to create a custom target
add_library(DearImGui STATIC
        ${dearimgui_SOURCE_DIR}/imgui.cpp
        ${dearimgui_SOURCE_DIR}/imgui_demo.cpp
        ${dearimgui_SOURCE_DIR}/imgui_draw.cpp
        ${dearimgui_SOURCE_DIR}/imgui_tables.cpp
        ${dearimgui_SOURCE_DIR}/imgui_widgets.cpp
        # SDL3 backend
        ${dearimgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${dearimgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
add_library(DearImGui::DearImGui ALIAS DearImGui)
target_include_directories(DearImGui PUBLIC
        ${dearimgui_SOURCE_DIR}
        ${dearimgui_SOURCE_DIR}/backends
)

target_link_libraries(DearImGui PUBLIC SDL3::SDL3 Vulkan::Headers)

add_library(stb INTERFACE
        ${stb_SOURCE_DIR}/stb_image.h
        # Add the sources for stb here as necessary
)
add_library(std::stb ALIAS stb)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})