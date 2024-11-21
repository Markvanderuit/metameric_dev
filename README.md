# Metameric

## Introduction

This repository contains the source code of our spectral uplifting editor, which accompanies our recent paper "**Controlled Spectral Uplifting for Indirect-Light-Metamerism**" (link to [proceedings](https://dx.doi.org/https://doi.org/10.1145/3680528.3687698), [university page](http://graphics.tudelft.nl/Publications-new/2024/RE24), [author page](https://markvanderuit.nl/publications/2024-11-19-paper-spectral)), which we'll present at Siggraph Asia 2024.

With this editor, we show how an artist-controllable spectral uplifting might actually be a great addition to the artist toolbox. The editor allows us to use (and misuse) spectral metamerism. In a spectral scene, where color occurs at the camera, not the asset, the color behavior of objects can be extremely underdetermined. We happily use this property to essentially control the color of objects under indirect illumination, and create some weird stuff. For complete details on how this works, check out the paper!

We include several [example scenes](github.com/Markvanderuit/metameric_dev/tree/main/scenes), and provide windows/linux builds (that might even work most of the time!) on the [releases page](github.com/Markvanderuit/metameric_dev/releases/latest).

## Compilation

To get started, clone the repository and any submodules.

```bash
  git clone --recurse-submodules https://github.com/markvanderuit/metameric
```

To build the project, you will need a recent C++23 compiler (MSVC **VN**, Clang **18**, GCC **13** should work), OpenGL 4.6 and CMake 3.22+ or later required. Dependencies are bundled through VCPKG, submodules, or as packed binaries. Next, configure and compile the project using CMake:

```bash
  cmake -S . -B build
  cmake --build build --target metameric_editor
```

Upon completion, you should find an executable as `build/bin/metameric_editor`.


> **Note** 
> Some Unix systems may require X11/Wayland development packages to be present for certain dependencies. VCPKG provides information on what dependencies you should install using your system's package manager. For GLFW, for example, the [GLFW compilation page](https://www.glfw.org/docs/3.3/compile.html) lists the necessary packages.

## Spectral editing

When you first run the editor (and all shaders compiled succesfully), you are greeted by an example scene: a simple cornell box. The editor has three main parts you can interact with.

1. **Scene components (left).** Objects, emitters and views can be placed and tweaked. The only important bit is the list of upliftings; each scene object must have an associated uplifting, which translates the object's RGB albedo together with spectral constraints into spectral reflectance data. You can see that the default uplifting in this scene has a single constraint already. Click the **edit** button to edit this constraint's behavior.
2. **The viewport (right).** Use middle mouse to pan, right mouse to rotate, and the mouse wheel to zoom your camera and move around the scene. Use left mouse to interact with spectral constraints on scene positions, if any are visible. If you clicked **edit** earlier, a single spectral constraint should now be marked as a blue dot in the cornell box' red shadow.
3. **The mismatch editor (floating).** When you clicked **edit**, this window popped up. Here, you see (top) the generated spectral reflectance at that blue marker position. Below that (middle) are input and constraint colors imposed on the underlying scene data. You can use the editor (bottom) to tweak the bottom-most constraint color. This is the color that spectral uplifting should produce at that blue scene marker. Simply pick and drag the point in the mismatch volume to edit the cornell box' shadow color.

IMAGE HERE

### Other editor features

If you play around with the editor a bit, you may find;

- Use `View > Render to File` to render a specific scene view to a exr file.
- Use `View > Path Measure Tool` to turn your mouse into a tiny spectrometer. You can see the spectral radiance going through your mouse into the camera. In addition, the renderer visualizes the light paths used to build this radiance estimate.

## Citation

Please cite the following paper if you found any of this useful in your research:

```bibtex
@InProceedings { RE24,
  author       = "Ruit, Mark van de and Eisemann, Elmar",
  title        = "Controlled Spectral Uplifting for Indirect-Light-Metamerism",
  booktitle    = "SIGGRAPH Asia 2024 Conference Papers",
  year         = "2024",
  publisher    = "ACM",
  doi          = "https://doi.org/10.1145/3680528.3687698",
  url          = "http://graphics.tudelft.nl/Publications-new/2024/RE24"
}
```

## License and third party software

The source code in this repository is released under the *GNU General Public License Version 3* (GPLv3). However, all third-party software packages used in this repository are governed by their respective licenses.

Without the following list of excellent libraries, this project would not exist:
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