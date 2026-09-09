# Gooblebox

**Gooblebox** is a Wayland compositor implementing the concept of **fractal tiling window environments**. 
Unlike standard desktop environments, Gooblebox arranges workspaces inside a 3D scene, allowing users to nest workspaces within other workspaces infinitely, complete with fluid transitions and full 3D rotation.

---

## Core Concepts

* **Fractal Workspaces**: Workspaces can be nested inside other workspaces infinitely. Every workspace acts as an independent "microverse" containing its own windows and sub-workspaces.
* **Automated Tiling**: Windows within any given workspace layer are automatically arranged using a dynamic tiling engine.
* **3D Space Navigation**: The entire desktop environment exists as a 3D scene. Users can rotate, zoom, and navigate between different workspace depths.
* **Fluid Transitions**: Spatial shifts and deep-nesting navigation are accompanied by smooth visual animations.

---

## Technical Stack

Gooblebox is built from the ground up for modern Linux graphics subsystems using the following technologies:

* **Language**: C++17
* **Windowing System**: Wayland (via `wlroots`)
* **Graphics API**: OpenGL ES 2.0 / EGL
* **Build System**: CMake

### Key Dependencies
* `libwlroots`
* `libwayland`
* `libxkbcommon`
* `libdrm`
* `libegl`
* `libgles2`
* `libseat`

---

## Building from Source

### Prerequisites

Ensure you have a C++17 compatible compiler, CMake, and the development headers for all key dependencies installed via your distribution's package manager.

### Compilation

```bash
# Clone the repository
git clone https://github.com
cd gooblebox

# Create and enter the build directory
mkdir build && cd build

# Configure and build the project
cmake ..
make
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
