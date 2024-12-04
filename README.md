# Controlled Spectral Uplifting for Indirect-Light Metamerism

![teaser_1](/resources//assets/teaser_1.png)


This repository contains the source code of our spectral uplifting editor *Metameric*, which accompanies our recent paper "**Controlled Spectral Uplifting for Indirect-Light-Metamerism**" (link to [proceedings](https://dx.doi.org/https://doi.org/10.1145/3680528.3687698), [university page](http://graphics.tudelft.nl/Publications-new/2024/RE24), [author page](https://markvanderuit.nl/publications/2024-11-19-paper-spectral)), presented at Siggraph Asia 2024.

With this demo editor, we show how artist-controllable spectral uplifting might be a great addition to the artist toolbox. Our method allows us to use (and misuse) spectral metamerism. In a spectral renderer with spectral uplifting, color occurs at the camera. The actual color behavior of objects is extremely underdetermined. We happily use this property to change the uplifted appearance of objects under complex indirect illumination, while keeping the RGB input intact.
We include several [example scenes](./scenes) with different spectral behaviors, and provide windows/linux builds (that might even work sometimes!) on the [releases page](./releases/latest).

For full details on how our method works, check out the paper, and feel free to contact one of the authors ([Mark van de Ruit](https://www.markvanderuit.nl), [Elmar Eisemann](https://graphics.tudelft.nl/~eisemann/)) if you have questions.

![teaser_2](/resources//assets/teaser_2.png)

## Compilation

To build the project, you'll need a recent C++23 compiler (e.g. MSVC **17.12** or GCC **13**), and OpenGL 4.6 and CMake 3.22+. Dependencies are bundled through VCPKG, submodules, and packed binaries. 

As a first step, clone the repository and any submodules.

```bash
  git clone --recurse-submodules https://github.com/markvanderuit/metameric_dev
```

Next, simply configure and compile the project using CMake:

```bash
  cmake -S metameric_dev -B build
  cmake --build build --config Release --target metameric_editor
```

If compilation succeeds without any errors, you should find an executable at `build/bin/metameric_editor`.

> **Note** 
> Some Unix systems may require X11/Wayland development packages for certain dependencies. VCPKG provides information on what you should install using your own package manager. E.g. for GLFW, the [GLFW compilation page](https://www.glfw.org/docs/3.3/compile.html) lists a number of required packages.

## Spectral editing

When you first run the editor (and all shaders compiled succesfully), you are greeted by an example scene: a simple cornell box. The editor has three main parts you can interact with.

1. **Scene components (left).** Objects, emitters and views can be placed and tweaked. The only important bit is the list of upliftings; each scene object must have an associated uplifting, which translates the object's RGB albedo together with spectral constraints into spectral reflectance data. You can see that the default uplifting in this scene has a single constraint already. Click the **edit** button to edit this constraint's behavior.
2. **The viewport (right).** Use middle mouse to pan, right mouse to rotate, and the mouse wheel to zoom your camera and move around the scene. Use left mouse to interact with spectral constraints on scene positions, if any are visible. If you clicked **edit** earlier, a single spectral constraint should now be marked as a blue dot in the cornell box' red shadow.
3. **The mismatch editor (floating).** When you clicked **edit**, this window popped up. Here, you see (top) the generated spectral reflectance at that blue marker position. Below that (middle) are input and constraint colors imposed on the underlying scene data. You can use the editor (bottom) to tweak the bottom-most constraint color. This is the color that spectral uplifting should produce at that blue scene marker. Simply pick and drag the point in the mismatch volume to edit the cornell box' shadow color.

![til](./resources/assets/example.gif)

### Other editor features

If you play around with the editor a bit, there are some minor features that are useful:

- Use `View > Render to file` to render a scene view to a .exr file.
- Use `View > Path measure tool` to turn your mouse into a spectrometer. You can see the spectral radiance going through your mouse into the camera. In addition, the renderer visualizes the light paths used to build this radiance estimate. The same path measures are used to build indirect light constraints.
- Use `File > Import > Mesh/Image` to import a .obj scene and texture data. The OBJ importer is rather incomplete, and it may ignore some of your scene data.

## Citation

If you found any of this useful in your research, please cite the following paper:

```bibtex
@InProceedings { RE24,
  author       = "van de Ruit, Mark and Eisemann, Elmar",
  title        = "Controlled Spectral Uplifting for Indirect-Light-Metamerism",
  booktitle    = "SIGGRAPH Asia 2024 Conference Papers",
  year         = "2024",
  publisher    = "ACM",
  doi          = "https://doi.org/10.1145/3680528.3687698",
  url          = "http://graphics.tudelft.nl/Publications-new/2024/RE24"
}
```

## License and third party software

The source code in this repository is released under the *GNU General Public License Version 3* (GPLv3).

All third-party software packages used in this repository are governed by their respective licenses. Without the following excellent libraries, this project would not exist:
[AutoDiff](https://github.com/autodiff/autodiff),
[Dear ImGui](https://github.com/ocornut/imgui),
[Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page), 
[fmt](https://github.com/fmtlib/fmt), 
[GLAD](https://glad.dav1d.de/),
[GLFW](https://www.glfw.org/),
[GLM](https://glm.g-truc.net/0.9.9/),
[Glslang](https://github.com/KhronosGroup/glslang), 
[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo), 
[ImPlot](https://github.com/epezent/implot),
[Intel Embree](https://www.embree.org/),
[JSON for Modern C++](https://github.com/nlohmann/json),
[MeshOptimizer](https://github.com/zeux/meshoptimizer),
[NLOpt](https://github.com/stevengj/nlopt).
[Qhull](http://www.qhull.org/),
[RapidOBJ](https://github.com/guybrush77/rapidobj),
[SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross), 
[SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools), 
[stb](https://github.com/nothings/stb),
[TinyEXR](https://github.com/syoyo/tinyexr),
[TinyFileDialogs](https://sourceforge.net/projects/tinyfiledialogs/),
[TBB](https://github.com/oneapi-src/oneTBB),
[Tracy](https://github.com/wolfpld/tracy),
[Vcpkg](https://github.com/microsoft/vcpkg),
[zlib](https://zlib.net/),
[zstr](https://github.com/mateidavid/zstr)
