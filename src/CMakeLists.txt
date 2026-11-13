set(ENGINE_NAME Ephemeral)
message(STATUS ${ENGINE_NAME})
add_executable(${ENGINE_NAME} ephemeral/Main.cpp)
target_compile_features(${ENGINE_NAME} PRIVATE cxx_std_20)

target_include_directories(
        ${ENGINE_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_subdirectory(ephemeral/vulkan)
add_subdirectory(ephemeral/utils)

target_link_libraries(
        ${ENGINE_NAME} PRIVATE
        ${PROJECT_NAME}-StrictWarnings
        EphemeralRenderer_Internal
        EphemeralUtils_Internal
)

set(EPHEMERAL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ------- SHADER COMPILATION(vkguide.dev) ---------------

#find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
#
#file(GLOB_RECURSE GLSL_SOURCE_FILES
#        "${CMAKE_CURRENT_LIST_DIR}/shaders/*.frag"
#        "${CMAKE_CURRENT_LIST_DIR}/shaders/*.vert"
#        "${CMAKE_CURRENT_LIST_DIR}/shaders/*.comp"
#)
#
#foreach (GLSL ${GLSL_SOURCE_FILES})
#    message(STATUS "BUILDING SHADER")
#    get_filename_component(FILE_NAME ${GLSL} NAME)
#    set(SPIRV "${CMAKE_CURRENT_LIST_DIR}/shaders/${FILE_NAME}.spv")
#    message(STATUS ${GLSL})
#    add_custom_command(
#            OUTPUT ${SPIRV}
#            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
#            DEPENDS ${GLSL})
#    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
#endforeach (GLSL)
#
#add_custom_target(
#        Shaders
#        DEPENDS ${SPIRV_BINARY_FILES}
#)

#if (WIN32)
#    add_custom_command(TARGET ${ENGINE_NAME} POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${ENGINE_NAME}> $<TARGET_FILE_DIR:${ENGINE_NAME}>
#            COMMAND_EXPAND_LISTS
#    )
#endif ()
