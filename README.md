# vkhrt

A work-in-progress hair ray-tracer built using Vulkan and the `VK_KHR_ray_tracing_pipeline` extension with the aim to compare different hair geometry rendering algorithms. The renderer supports a wide range of 3D hair model formats using polylines via Assimp, enabling the import of virtually any format supported by the library.

<img width="816" height="932" alt="image" src="https://github.com/user-attachments/assets/df03d8a1-ac92-4359-817f-122ba89104a0" />

The project currently supports 3 out of 4 planned hair rendering techniques.

- The [`Phantom Hair Ray Intersector`](https://research.nvidia.com/sites/default/files/pubs/2018-08_Phantom-Ray-Hair-Intersector//Phantom-HPG%202018.pdf) researched and developed by Alexander Reshtov and David Luebke.
- Nvidia's [`Disjoint Orthogonal Triangle Strips`](https://developer.nvidia.com/blog/render-path-traced-hair-in-real-time-with-nvidia-geforce-rtx-50-series-gpus).
- Nvidia's [`Linear Swept Sphere`](https://developer.nvidia.com/blog/render-path-traced-hair-in-real-time-with-nvidia-geforce-rtx-50-series-gpus).

The only one left and currently work-in-progress algorithm is based on Unreal Engine 5's voxalized foliage rendering showcased in [the Witcher 4 demo](https://www.youtube.com/watch?v=Nthv4xF_zHU), where the hair will be represented using voxels, will use LODs and try to imitate accurate normals for shading purposes.

## Showcase

### Phantom Ray Hair Intersector

<img width="1559" height="800" alt="image" src="https://github.com/user-attachments/assets/c7d91009-7ee0-48cc-8459-2a15925fcbf0" />

<img width="505" height="492" alt="image" src="https://github.com/user-attachments/assets/752427f6-8db6-4ba6-a896-d795c90514a4" />

### Linear Swept Spheres

<img width="1483" height="650" alt="image" src="https://github.com/user-attachments/assets/eed3815f-182e-4266-a884-409999be4ee4" />

<img width="547" height="478" alt="image" src="https://github.com/user-attachments/assets/82a0dcf0-fc9c-471f-a175-b86ba152056d" />

### Disjoint Orthogonal Triangle Strips

<img width="1441" height="657" alt="image" src="https://github.com/user-attachments/assets/55a2b666-0d4c-40f3-aafa-41b101b28b61" />

<img width="498" height="571" alt="image" src="https://github.com/user-attachments/assets/bbbda992-abcb-465d-bf95-7428e8a9fadd" />


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

To build the project, use any compiler you want. But either Clang or GCC is recommended as these 2 compilers are tested regularly.
All the build files can be found in the root directory inside the `build` folder.


## Libraries

- [Vulkan](https://vulkan.lunarg.com/sdk/home)
- [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [SDL](https://github.com/libsdl-org/SDL)
- [glm](https://github.com/g-truc/glm)
- [assimp](https://github.com/assimp/assimp)
- [stb](https://github.com/nothings/stb)
- [spdlog](https://github.com/gabime/spdlog)
- [ImGui](https://github.com/ocornut/imgui)

