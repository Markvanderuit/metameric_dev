# Metameric

## Introduction

This repository contains the implementation of our spectral uplifting toolkit, which can be used to author compact spectral textures from RGB inputs. A single-file plugin is provided for interfacing with [Mitsuba 3](https://github.com/mitsuba-renderer/mitsuba3). For details and renders, check out our recent paper "*Metameric: Spectral Uplifting via Controllable Color Constraints*" (**journal link**, **author link**).

## Compilation

To get started, first clone the repository and submodules.

```bash
  git clone --recurse-submodules https://github.com/markvanderuit/metameric
```

To build the project, you will need a viable C++20 compiler (MSVC **VN**, Clang **VN**, GCC **VN** should work), OpenGL 4.6 and CMake 3.22+ or later. All other dependencies are bundled through submodules and Vcpkg.

> **Note** 
> Some Unix systems may require X11/Wayland development packages to be present for [GLFW](https://www.glfw.org). Vcpkg provides steps to help install these, but if you run into any issues, refer to the [GLFW compilation page](https://www.glfw.org/docs/3.3/compile.html).

Next, configure the project using CMake and your compilation path of choice. E.g., when using Make:

```bash
  mkdir build
  cd build
  cmake ../metameric
  make
```

Once this has completed, you should find an executable on the path `build/bin/metameric`.

## Usage

...

## Mitsuba plugin

...

## Citation

Please cite the following paper if you found it useful in your research:

**TODO: add bibtex**

## License and third party software

The source code in this repository is released under the MIT License. 
However, all third-party software libraries are governed by their respective licenses.
Without the following libraries, this project would not exist:
[CLP](https://github.com/coin-or/Clp), 
[cxxopts](https://github.com/jarro2783/cxxopts),
[Dear ImGui](https://github.com/ocornut/imgui),
[Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page), 
[fmt](https://github.com/fmtlib/fmt), 
[GLAD](https://glad.dav1d.de/),
[GLFW](https://www.glfw.org/),
[GLM](https://glm.g-truc.net/0.9.9/),
[Glslang](https://github.com/KhronosGroup/glslang), 
[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo), 
[ImPlot](https://github.com/epezent/implot),
[JSON for Modern C++](https://github.com/nlohmann/json),
[Native File Dialog](https://github.com/mlabbe/nativefiledialog), 
[OpenMesh](https://openmesh.org),
[Qhull](http://www.qhull.org/),
[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross), 
[SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools), 
[stb](https://github.com/nothings/stb),
[TinyEXR](https://github.com/syoyo/tinyexr),
[Tracy](https://github.com/wolfpld/tracy),
[Vcpkg](https://github.com/microsoft/vcpkg),
[zlib](https://zlib.net/),
[zstr](https://github.com/mateidavid/zstr)