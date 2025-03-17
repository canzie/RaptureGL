# LiDAR Game

## External Libraries Setup

This project uses several external libraries for OpenGL rendering and other functionality. All libraries are configured in the Engine/vendor directory.

### Setting up the Vendor Directory

The project expects the following directory structure:

```
Engine/
  vendor/
    GLFW/
      include/
        GLFW/
          glfw3.h
          glfw3native.h
      lib/
        glfw3.lib (static library) 
        # or alternatively:
        # glfw3dll.lib (dynamic library)
      bin/ (only needed if using dynamic library)
        glfw3.dll
    glad/
      include/
        glad/
          glad.h
        KHR/
          khrplatform.h
      src/
        glad.c
    glm/
      glm/
        glm.hpp
        # other GLM headers
    imgui/
      # ImGui source files
      backends/
        imgui_impl_opengl3.cpp
        imgui_impl_glfw.cpp
    entt/
      include/
        entt/
          entt.hpp
    spdlog/
      include/
        spdlog/
          spdlog.h
    stb_image/
      stb_image.h
```

### Obtaining External Libraries

1. **GLFW**: Download from [the official website](https://www.glfw.org/download.html)
2. **GLAD**: Generate from [the GLAD web service](https://glad.dav1d.de/) (OpenGL 4.6 Core profile)
3. **GLM**: Download from [GitHub](https://github.com/g-truc/glm)
4. **ImGui**: Download from [GitHub](https://github.com/ocornut/imgui)
5. **EnTT**: Download from [GitHub](https://github.com/skypjack/entt)
6. **spdlog**: Download from [GitHub](https://github.com/gabime/spdlog)
7. **stb_image**: Download from [GitHub](https://github.com/nothings/stb)

### Building with CMake

Once the vendor directories are set up correctly:

1. Create a build directory: `mkdir build && cd build`
2. Configure the project: `cmake ..`
3. Build the project: `cmake --build .`

## Project Structure

- `Engine/`: Core engine code
  - `src/`: Engine source files
  - `vendor/`: External dependencies
- `Editor/`: Editor application code
- `build/`: Build output directory