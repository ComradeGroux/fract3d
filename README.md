<div align="center">

# fract3d

**A real-time 3D fractal explorer, built from scratch in C++ with Vulkan.**

Fly through infinitely detailed fractal landscapes — move, zoom, and rotate the camera in real time, rendered directly on the GPU.

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-AC162C?logo=vulkan&logoColor=white)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Wayland-333333?logo=linux&logoColor=white)](#requirements)
[![License](https://img.shields.io/badge/license-GPLv3-blue)](#license)

</div>

---

## Overview

**fract3d** is a real-time 3D fractal renderer written in modern C++ and Vulkan. Fractals are computed and rendered directly on the GPU, letting you navigate freely through them — translating, zooming, and rotating the camera — while the fractal detail unfolds in real time.

The project is built with a low-level, hand-rolled Vulkan renderer (no engine, no abstraction layer) using dynamic rendering, synchronization2, and a self-managed build pipeline that fetches and compiles its own dependencies.

## Why this project?

This project is first and foremost a **learning exercise**. It's built from scratch, without a game engine or a Vulkan abstraction library, in order to get a real, hands-on understanding of:

- **Modern C++** (C++17): RAII, smart resource management, modern initialization patterns, build tooling
- **Vulkan**, in depth: instance/device setup, memory management, synchronization, dynamic rendering, and the graphics pipeline — including all the ways it can (and will) go wrong along the way

## A note on AI usage

This project is coded entirely by hand — **all the code is my own**, written to actually learn C++ and Vulkan rather than to just ship something that works.

AI (Claude) is used strictly as a **support tool**, the way you'd use documentation, a search engine, or a knowledgeable friend:

- 🐛 Debugging help — understanding compiler/linker errors, Vulkan validation layer messages, build issues
- 📚 Explaining concepts — C++ features, Vulkan API design, graphics programming theory
- 📦 Guidance on third-party libraries (SDL3, VMA, volk, GLM, shaderc...) and how to use them correctly

**No AI-generated code is used in this project.** No feature, function, or implementation was written by an AI — every line is typed and understood by me. If I ever change this policy, this section will be updated to reflect it.

## Features

- 🌀 **Real-time 3D fractal rendering** on the GPU
- 🎮 **Free camera navigation** — move, zoom, and rotate through the fractal
- ⚡ **Modern Vulkan 1.3** pipeline (dynamic rendering, synchronization2, no render passes/framebuffers)
- 🪟 Windowing and input via **SDL3**, with native **Wayland** support
- 🧠 Efficient GPU memory management via **AMD VMA**
- 🧩 Shaders compiled to SPIR-V at build time and embedded directly into the binary
- 🛠️ Self-contained build system — dependencies are fetched and compiled automatically, no manual setup required

## Demo

<div align="center">

*Screenshots / GIFs coming soon.*

</div>

## Controls

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Move the camera |
| Mouse | Rotate the camera |
| Scroll wheel | Zoom in / out |
| `Esc` | Quit |

> Controls are subject to change as the project evolves — check back here for updates.

## Requirements

- A GPU and driver with **Vulkan 1.3** support
- Linux with **Wayland**
- `g++` (C++17), `cmake`, `make`
- `curl`, `tar` (used by the build system to fetch dependencies)
- Vulkan development headers/loader for your distro (needed to link/run against the system Vulkan implementation)

Everything else — SDL3, GLM, volk, VulkanMemoryAllocator, and the shaderc/glslc shader compiler — is **downloaded and built automatically** by the Makefile on first build. No system-wide installation required.

## Getting Started

Clone the repository and build:

```bash
git clone https://github.com/<your-username>/fract3d.git
cd fract3d
make
```

On the first run, the build system will:
1. Download and build **SDL3** (statically, with Wayland/Vulkan support)
2. Download the **volk**, **VMA**, and **GLM** headers
3. Compile the GLSL shaders to SPIR-V, building `glslc` from source if it isn't available on your system
4. Compile and link the final binary

This can take a few minutes the first time — subsequent builds are incremental and much faster.

Run it with:

```bash
./fract3d
```

### Build targets

| Command | Description |
|---|---|
| `make` | Build the project (default target) |
| `make re` | Full rebuild (`fclean` + `all`) |
| `make clean` | Remove compiled object files |
| `make fclean` | Remove all build artifacts and the binary |
| `make dclean` | `fclean` + remove downloaded dependencies |

## Project Structure

```
fract3d/
├── src/                 # C++ source files
├── include/             # Project headers
├── shader/              # GLSL shader sources (.vert / .frag)
├── lib/                 # Fetched third-party dependencies (auto-generated)
├── build/               # Build artifacts: objects, compiled SPIR-V, SDL, shaderc (auto-generated)
└── Makefile
```

## Tech Stack

| Component | Purpose |
|---|---|
| [Vulkan](https://www.vulkan.org/) | Graphics API |
| [volk](https://github.com/zeux/volk) | Vulkan function loader |
| [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | GPU memory allocation |
| [SDL3](https://github.com/libsdl-org/SDL) | Windowing, input, and Vulkan surface creation |
| [GLM](https://github.com/g-truc/glm) | Math library (vectors, matrices, transforms) |
| [shaderc](https://github.com/google/shaderc) / `glslc` | GLSL → SPIR-V shader compilation |

## Roadmap

- [ ] Multiple fractal types (Mandelbulb, Menger sponge, etc.)
- [ ] Adjustable rendering/quality parameters at runtime
- [ ] Lighting and shading improvements
- [ ] On-screen UI for tweaking parameters live
- [ ] Configurable camera and movement speed

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).

## Author

Made with ❤️ and a lot of Vulkan validation errors.