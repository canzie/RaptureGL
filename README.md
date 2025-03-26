# LiDAR Game

A modern 3D game engine built with C++ and OpenGL.



### Rendering
- PBR (Physically Based Rendering) with support for:
  - Metallic-roughness workflow
  - Specular-glossiness workflow
  - Full PBR texture maps (albedo, normal, metallic, roughness, AO, emissive)
- Material instance system with runtime parameter overrides
- Efficient material library with serialization support

### Scene Management
- Flexible entity hierarchy system for complex scenes
- Efficient bounding box system for collision detection
- Smart scene serialization

### Asset Pipeline
- Efficient mesh loading with support for glTF
- Multi-threaded asset loading
- Material library with instance management
- Advanced texture management with automatic resource pooling
- Buffer pool management for optimal memory usage

## TODO's

### High Priority
- [ ] Deferred rendering pipeline
- [ ] Instanced mesh rendering for large scenes
- [ ] Advanced particle system with GPU acceleration
- [ ] Mouse picking system for editor interaction
- [ ] Post-processing pipeline

### Future Plans
- [ ] Physics engine integration
- [ ] Audio system
- [ ] Advanced serialization system
- [ ] Scripting system for runtime behavior
- [ ] Resource streaming system

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

## Project Structure
```
Engine/
  ├── src/
  │   ├── Buffers/        # Advanced buffer management
  │   ├── Camera/         # Camera system
  │   ├── Materials/      # PBR material system
  │   ├── Mesh/          # Mesh loading
  │   ├── Renderer/      # Core rendering
  │   ├── Scenes/        # Scene management
  │   ├── Shaders/       # Shader system
  │   └── Textures/      # Texture management
  └── vendor/            # External dependencies
```