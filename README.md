# LiDAR Game

A modern 3D game engine built with C++ and OpenGL. 
Created for learning 3D graphics and game engine architecures from scratch.



### Rendering
- PBR (Physically Based Rendering) with support for:
  - Metallic-roughness workflow
  - Specular-glossiness workflow
  - Full PBR texture maps (albedo, normal, metallic, roughness, AO, emissive)
- Support for both forward and deferred rendering pipelines
- Support for Point, Spot and Directional lights
- Traditional and Cascaded Shadow Mapping (CSM)
- Frustum Culling
- Modular and Asynchronous command queue system for creating streamlined render passes

### Scene Management
- Scenes consist out of Entities using the [entt ECS](https://github.com/skypjack/entt)
- Advanced project hierarchy, Project->Worlds->Scenes

### Asset Pipeline
- Asset Manager for centralized loading of any asset
- Efficient and custom glTF 2.0 loader
- Multi-threaded Texture loading
- Material System
- Buffer pools for efficient Vertex and Index buffer allocation


## Showcase of recent features

## Cascaded Shadow Mapping (CSM)

I recently finished(mostly) my implementation of CSM, it uses scene independant matrix transformations for the cascade ligthview matrices.
Uses a hybrid approach for spliting the frustum range with a lambda to bias either the logarithmic or linear splits

![CSM with visible cascades](screenshots/CSM_Example.PNG)

## Getting Started

### Requirements
- C++17/20 compiler
- CMake 3.15+
- OpenGL 4.6 Core Profile
- External libraries (see below)

### Dependencies
We use some great open-source libraries:
- GLFW for window management
- GLAD for OpenGL loading
- GLM for math
- ImGui for UI
- EnTT for ECS
- spdlog for logging
- stb_image for image loading
- YAML for serialization

### Building
```bash
# Build
mkdir build && cd build
cmake ..
cmake --build .
```
Or run the build script, then nagivate to the build folder: 
 - build\release\bin\Release
