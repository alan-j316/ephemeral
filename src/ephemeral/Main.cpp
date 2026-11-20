/**
 * @file Main.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief Main entry point.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include <ephemeral/vulkan/VkEngine.h>

static constexpr auto DEFAULT_ASSET = "assets/structure.glb";

int main(int argc, char* argv[])
{
    ephemeral::EphemeralEngine engine;
    engine.init("Ephemeral");

    // If any files are provided as arguments then these will be rendered.
    std::span<const char* const> args(argv, static_cast<size_t>(argc));
    args = args.subspan(1);
    if (args.size() > 0)
    {
        engine.load(args);
    }
    else
    {
        std::cout << "No assets passed.\nUsage: Ephemeral.exe <path-to-asset.(glb|gltf)\n";
        std::cout << "Loading default asset.\n";
        engine.load(std::span(&DEFAULT_ASSET, 1));
    }

    engine.run();
    engine.cleanup();

    return 0;
}
