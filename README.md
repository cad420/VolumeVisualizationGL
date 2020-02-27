# VolumeVisualizationGL

### An OpenGL 4.5 based large volume renderer.

## Feature:
----
1. **OpenGL 4.5 based:** 
All OpenGL APIs are DSA version. They are very clear and effective. Therefore, the source code is also a good example for learning the newer API, which covers the most common API on rendering.
2. **Native API almost in one file:**
Almost all OpenGL calls are explicitly written in *main()* function that you can easily read from start to end and understand the graphics pipeline configuration.
3. **Large Scale Volume Data Rendering:**
It can render serveral tera-bytes volume data interactivly.
4. **Flexible volume data format:** For better perfermance, you can define your own data format without recompiling the renderer. Just extents the data reader by the plugin ```IBlock3DPluginInterface ``` defined in **[VMCore][1]**

**Installation:**
----
### Requirements:
1. **Git**
2. **CMake**
3. **C++17 or heigher**


### Dependences:
The only external dependency is **[GLFW][2]**. 

Besides, it has dependent libraries **[VMCore][1]**,**[VMUtils][2]** and **[VMat][3]**, which are automatically installed by the cmake scripts
### Windows:
**[Vcpkg][5] is highly recommanded to install the GLFW**. 

```
git clone https://github.com/cad420/VolumeVisualizationGL.git --recursive
```
The project could be opened in **VS2019** or **VS2017** by **Open Folder** and compiled 

### Linux:
You can use any package manager you like to install **GLFW**.
After install the external dependences:

``` 
git clone https://github.com/cad420/VolumeVisualizationGL.git --recursive
```
You need cmake configuring and compiling as usual.

### macOS:
---
OpenGL is deprecated by Apple long ago. It has no latest version this project rely on.
Moreover, the performence of OpenGL on macOS is horrible. Forget it on macOS though it could be compiled successfully theratically as what to be done on Linux.

### Others:
---
 1. Please reference the command line parameters handler for further use.
 1. The project is just ported from **VisualMan** and not tested well. It only run with a small-scale data.
 2. The project do not integrate default data reader for any formats. If you need our data reader, please compile it from **[VMPlugins](https://gitub.com/cad420/VMPlugin)**, and put the shared library into the subdirectory *plugins* in the executable binary's directory.
 3. There is a OpenGL graphics driver issue when using Intel GPU. see https://software.intel.com/en-us/forums/graphics-driver-bug-reporting/topic/740117. I do not have other GPU now, so I try a alternative way. If you have other GPU, you can subsititute with better codes commented in rendering loop.

[1]: https://github.com/cad420/VMCore
[2]: https://github.com/cad420/VMUtils
[3]: https://github.com/cad420/VMat

[4]: https://github.com/glfw/glfw
[5]: https://github.com/microsoft/vcpkg