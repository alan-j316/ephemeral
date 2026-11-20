[![Unit Tests](https://github.com/alanabrahampkochumon-zqict/CPP_Project_Template/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/alanabrahampkochumon-zqict/CPP_Project_Template/actions/workflows/unit-tests.yml)

# Ephemeral

API-decoupled Vulkan-based rendering engine.

_NOTE: Only works on MSVC compiler_

## Screenshots

![Vulkan Engine](docs/assets/Screenshot.png)

# Prerequisites

Ensure the following tools are installed and available in your system PATH:

- <a href="https://cmake.org/download/" target="_blank">CMake</a> (3.25+): Build system generator.

- <a href="https://vulkan.lunarg.com/sdk/home" target="_blank">Vulkan</a> 1.3+: Graphics API.

- <a href="https://git-scm.com/install/windows" target="_blank">Git</a>: Required for FetchContent dependency
  management.

- <a href="https://git-scm.com/install/windows" target="_blank">Doxygen (Doxywizard)</a> (Optional): Required to
  generate HTML/PDF documentation.

- <a href="https://releases.llvm.org/download.html" target="_blank">Clang-Format</a> : Required for automated code style
  enforcement.

## Getting Started

### Configuration

Initialize the build system and download dependencies. Bash

`cmake -B build`

### Compilation

Compile the entire project, including tests and benchmarks. Bash

`cmake --build build`

Or use an IDE of choice.

## Program Usage

`Ephemeral.exe assets/structure.glb assets/basicmesh.glb`

`Ephemeral.exe` *(loads default scene, if present)*

**Note:** The executable is not standalone and must be placed in the parent directory of the `shaders/` folder;
missing shader binaries will cause pipeline creation failures. Assets can be loaded via command-line arguments.
Providing no valid assets will cause the program to exit/crash.

**Known Limitation:** Scene loading currently relies on explicit texture and mesh names inside the glTF/GLB file.
Assets with unnamed textures or meshes are not yet supported.

## Controls

- Use mouse for basic panning.
- W/S: Move forward/backward.
- A/D: Move left/right.
- Q/E: Move up/down.

## Developer Utilities

Run these targets to maintain code quality and documentation.

- Auto-Format: `cmake --build build --target format`

- Generate Docs: cmake `--build build --target docs`

## Credits

- [VkGuide.dev](https://vkguide.dev/)
- [Khronos Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/00_Introduction.html)
