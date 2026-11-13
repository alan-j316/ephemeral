
# Add source to this project's executable.
add_library(
        EphemeralUtils_Internal STATIC
        Camera.cpp
)

target_sources(
        EphemeralUtils_Internal
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
        Camera.h
        DeletionQueue.h
)


set_property(TARGET EphemeralUtils_Internal PROPERTY CXX_STANDARD 20)
target_include_directories(EphemeralUtils_Internal PUBLIC "${EPHEMERAL_ROOT_DIR}")

target_link_libraries(EphemeralUtils_Internal PUBLIC
        Falcon::Falcon
)

target_precompile_headers(
        EphemeralUtils_Internal
        PUBLIC
        <optional>
        <vector>
        <memory>
        <string>
        <vector>
        <unordered_map>
)