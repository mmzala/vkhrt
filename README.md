# vkhrt

A work-in-progress hair ray-tracer built using Vulkan and the `VK_KHR_ray_tracing_pipeline` extension. The renderer supports a wide range of 3D hair model formats using line primitives via Assimp, enabling the import of virtually any format supported by the library.

The first and currently in development hair rendering technique is the [`Phantom Hair Ray Intersector` researched and developed by Alexander Reshtov and David Luebke](https://research.nvidia.com/sites/default/files/pubs/2018-08_Phantom-Ray-Hair-Intersector//Phantom-HPG%202018.pdf). In the future I plan to add [Nvidia's `Disjoint Orthogonal Triangle Strips`, `Linear Swept Sphere`](https://developer.nvidia.com/blog/render-path-traced-hair-in-real-time-with-nvidia-geforce-rtx-50-series-gpus) and voxalized hair rendering based on [Unreal Engine 5's voxalized foliage rendering showcased in the Witcher 4 demo](https://www.youtube.com/watch?v=Nthv4xF_zHU).

## Build Instructions

The project currently only supports Windows. First you'll have to install the [VulkanSDK](https://vulkan.lunarg.com/sdk/home).

Clone the project to your device:
```
git clone --recursive https://github.com/mmzala/vkhrt.git
```

To setup the project use the following cmake command:
```
cmake CMakeLists.txt
```

To build the project, use the [Clang](https://clang.llvm.org/get_started.html) compiler.
All of the build files can be found in the root directory inside the `build` folder.

## Libraries

- [Vulkan](https://vulkan.lunarg.com/sdk/home)
- [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [SDL](https://github.com/libsdl-org/SDL)
- [glm](https://github.com/g-truc/glm)
- [assimp](https://github.com/assimp/assimp)
- [stb](https://github.com/nothings/stb)
- [spdlog](https://github.com/gabime/spdlog)
