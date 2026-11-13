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

// int argc, char* argv[]
int main()
{
    ephemeral::EphemeralEngine engine;

    engine.init();
    engine.run();
    engine.cleanup();

    return 0;
}
