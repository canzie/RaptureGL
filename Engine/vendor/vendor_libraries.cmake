# Vendor Libraries Configuration

# Create a vendor interface target
add_library(vendor_libraries INTERFACE)

# Find system dependencies
find_package(OpenGL REQUIRED)

# Print the current directory for debugging
message(STATUS "Current source dir: ${CMAKE_CURRENT_SOURCE_DIR}")

# ==================== GLFW ====================
# Only look in Engine/vendor for libraries
set(GLFW_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/GLFW")
message(STATUS "Looking for GLFW at ${GLFW_DIR}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

if(EXISTS "${GLFW_DIR}/include/GLFW/glfw3.h")
    message(STATUS "GLFW headers found")
    
    # Check if using pre-built libraries or building from source
    if(EXISTS "${GLFW_DIR}/lib/glfw3.lib")
        message(STATUS "Using pre-built GLFW static library")
        add_library(glfw STATIC IMPORTED)
        set_target_properties(glfw PROPERTIES
            IMPORTED_LOCATION "${GLFW_DIR}/lib/glfw3.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_DIR}/include"
        )
    elseif(EXISTS "${GLFW_DIR}/lib/glfw3dll.lib")
        message(STATUS "Using pre-built GLFW dynamic library")
        add_library(glfw SHARED IMPORTED)
        set_target_properties(glfw PROPERTIES
            IMPORTED_IMPLIB "${GLFW_DIR}/lib/glfw3dll.lib"
            IMPORTED_LOCATION "${GLFW_DIR}/bin/glfw3.dll"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_DIR}/include"
        )
    elseif(EXISTS "${GLFW_DIR}/CMakeLists.txt")
        message(STATUS "Building GLFW from source")
        add_subdirectory(${GLFW_DIR} glfw EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "GLFW library not found in ${GLFW_DIR}/lib")
    endif()
else()
    message(FATAL_ERROR "GLFW headers not found in ${GLFW_DIR}/include")
endif()

# ==================== GLAD ====================
# Only look in Engine/vendor for libraries
set(GLAD_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/glad")
message(STATUS "Looking for GLAD at ${GLAD_DIR}")

if(EXISTS "${GLAD_DIR}/include/glad/glad.h" AND EXISTS "${GLAD_DIR}/src/glad.c")
    message(STATUS "GLAD found")
    add_library(glad STATIC ${GLAD_DIR}/src/glad.c)
    target_include_directories(glad PUBLIC ${GLAD_DIR}/include)
    target_link_libraries(glad PRIVATE ${OPENGL_LIBRARIES})
else()
    message(FATAL_ERROR "GLAD not found in ${GLAD_DIR}")
endif()

# ==================== GLM ====================
# Only look in Engine/vendor for libraries
set(GLM_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/glm")
message(STATUS "Looking for GLM at ${GLM_DIR}")

# Try to find GLM in different possible structures
if(EXISTS "${GLM_DIR}/glm/glm.hpp")
    message(STATUS "GLM found")
    # GLM directly in vendor/glm
elseif(EXISTS "${GLM_DIR}/glm-master/glm/glm.hpp")
    set(GLM_DIR "${GLM_DIR}/glm-master")
    message(STATUS "GLM found in glm-master")
else()
    # Search recursively for glm.hpp
    file(GLOB_RECURSE GLM_HEADER "${GLM_DIR}/**/glm/glm.hpp")
    
    if(GLM_HEADER)
        # Get the directory containing glm/glm.hpp
        get_filename_component(GLM_PATH ${GLM_HEADER} DIRECTORY)
        get_filename_component(GLM_DIR ${GLM_PATH} DIRECTORY)
        message(STATUS "Found GLM using recursive search at ${GLM_DIR}")
    else()
        message(FATAL_ERROR "GLM not found. Please place GLM in Engine/vendor/glm directory.")
    endif()
endif()

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${GLM_DIR})

# ==================== ImGui ====================
# Only look in Engine/vendor for libraries
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/imgui")
message(STATUS "Looking for ImGui at ${IMGUI_DIR}")

if(EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(STATUS "ImGui found")
    file(GLOB IMGUI_SOURCES ${IMGUI_DIR}/*.cpp)
    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC ${IMGUI_DIR})
    
    # ImGui OpenGL backend
    if(EXISTS "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp")
        message(STATUS "ImGui OpenGL/GLFW backends found")
        target_sources(imgui PRIVATE 
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        )
        target_link_libraries(imgui PRIVATE glad glfw)
    else()
        message(STATUS "ImGui OpenGL/GLFW backends not found, creating empty files")
        file(WRITE ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp "// Generated empty file")
        file(WRITE ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp "// Generated empty file")
        target_sources(imgui PRIVATE 
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        )
    endif()
else()
    message(FATAL_ERROR "ImGui not found in ${IMGUI_DIR}. Please clone from https://github.com/ocornut/imgui.git (docking branch)")
endif()

# ==================== EnTT ====================
# Only look in Engine/vendor for libraries
set(ENTT_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/entt")
message(STATUS "Looking for EnTT at ${ENTT_DIR}")

if(EXISTS "${ENTT_DIR}/include/entt/entt.hpp")
    message(STATUS "EnTT found")
    add_library(entt INTERFACE)
    target_include_directories(entt INTERFACE ${ENTT_DIR}/include)
else()
    # Might be nested in a subdirectory
    file(GLOB_RECURSE ENTT_HEADER "${ENTT_DIR}/**/include/entt/entt.hpp")
    
    if(ENTT_HEADER)
        get_filename_component(ENTT_PATH ${ENTT_HEADER} DIRECTORY)
        get_filename_component(ENTT_INCLUDE_DIR ${ENTT_PATH} DIRECTORY)
        get_filename_component(ENTT_DIR ${ENTT_INCLUDE_DIR} DIRECTORY)
        
        message(STATUS "EnTT found using recursive search at ${ENTT_DIR}")
        add_library(entt INTERFACE)
        target_include_directories(entt INTERFACE ${ENTT_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "EnTT not found in ${ENTT_DIR}")
    endif()
endif()

# ==================== spdlog ====================
# Only look in Engine/vendor for libraries
set(SPDLOG_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/spdlog")
message(STATUS "Looking for spdlog at ${SPDLOG_DIR}")

# Configure spdlog build options
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "Build spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "Generate spdlog install target" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as a shared library" FORCE)

# Check if spdlog has its own CMakeLists.txt (source version)
if(EXISTS "${SPDLOG_DIR}/CMakeLists.txt")
    message(STATUS "Building spdlog from source")
    add_subdirectory(${SPDLOG_DIR} spdlog EXCLUDE_FROM_ALL)
elseif(EXISTS "${SPDLOG_DIR}/spdlog-1.x/CMakeLists.txt")
    message(STATUS "Building spdlog-1.x from source")
    add_subdirectory(${SPDLOG_DIR}/spdlog-1.x spdlog EXCLUDE_FROM_ALL)
else()
    # Fallback to manual building if no CMakeLists.txt
    file(GLOB SPDLOG_SOURCES 
        "${SPDLOG_DIR}/src/*.cpp"
        "${SPDLOG_DIR}/spdlog-1.x/src/*.cpp"
    )
    
    if(SPDLOG_SOURCES)
        message(STATUS "Building spdlog manually from sources")
        add_library(spdlog STATIC ${SPDLOG_SOURCES})
        
        # Find include directory
        if(EXISTS "${SPDLOG_DIR}/include/spdlog/spdlog.h")
            target_include_directories(spdlog PUBLIC ${SPDLOG_DIR}/include)
        elseif(EXISTS "${SPDLOG_DIR}/spdlog-1.x/include/spdlog/spdlog.h")
            target_include_directories(spdlog PUBLIC ${SPDLOG_DIR}/spdlog-1.x/include)
        endif()
        
        target_compile_definitions(spdlog PUBLIC 
            SPDLOG_COMPILED_LIB
            FMT_HEADER_ONLY=0
        )
    else()
        message(FATAL_ERROR "spdlog source files not found. Please clone spdlog with source files.")
    endif()
endif()

# Make sure we set the correct definition for compiled mode
target_compile_definitions(spdlog PUBLIC 
    SPDLOG_COMPILED_LIB
    FMT_HEADER_ONLY=0
)

# ==================== stb_image ====================
# Only look in Engine/vendor for libraries
set(STB_IMAGE_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/stb_image")
message(STATUS "Looking for stb_image at ${STB_IMAGE_DIR}")

if(EXISTS "${STB_IMAGE_DIR}/stb_image.h")
    message(STATUS "stb_image found")
    add_library(stb_image INTERFACE)
    target_include_directories(stb_image INTERFACE ${STB_IMAGE_DIR})
else()
    # Try to find recursively
    file(GLOB_RECURSE STB_IMAGE_HEADER "${CMAKE_SOURCE_DIR}/Engine/vendor/**/stb_image.h")
    
    if(STB_IMAGE_HEADER)
        get_filename_component(STB_IMAGE_DIR ${STB_IMAGE_HEADER} DIRECTORY)
        message(STATUS "stb_image found using recursive search at ${STB_IMAGE_DIR}")
        add_library(stb_image INTERFACE)
        target_include_directories(stb_image INTERFACE ${STB_IMAGE_DIR})
    else()
        message(FATAL_ERROR "stb_image not found in ${STB_IMAGE_DIR}")
    endif()
endif()

# ==================== yaml-cpp ====================
# Only look in Engine/vendor for libraries
set(YAML_CPP_DIR "${CMAKE_SOURCE_DIR}/Engine/vendor/yaml-cpp")
message(STATUS "Looking for yaml-cpp at ${YAML_CPP_DIR}")

# Configure yaml-cpp build options
set(YAML_CPP_BUILD_CONTRIB ON CACHE BOOL "Enable yaml-cpp contrib in library" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Enable parse tools" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp shared library" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Enable yaml-cpp tests" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "Enable generation of yaml-cpp install targets" FORCE)

if(EXISTS "${YAML_CPP_DIR}/CMakeLists.txt")
    message(STATUS "Building yaml-cpp from source")
    add_subdirectory(${YAML_CPP_DIR} yaml-cpp EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "yaml-cpp not found in ${YAML_CPP_DIR}")
endif()

# ==================== Link all libraries to vendor_libraries ====================
target_link_libraries(vendor_libraries INTERFACE
    glad
    glfw
    glm
    imgui
    entt
    spdlog
    stb_image
    yaml-cpp
    ${OPENGL_LIBRARIES}
)

# Add Windows-specific libraries for GLFW
if(WIN32)
    target_link_libraries(vendor_libraries INTERFACE gdi32 user32 shell32)
endif()

# Include directories for all vendor libraries
target_include_directories(vendor_libraries INTERFACE
    ${GLAD_DIR}/include
    ${GLFW_DIR}/include
    ${GLM_DIR}
    ${IMGUI_DIR}
    ${ENTT_DIR}/include
    ${SPDLOG_DIR}/include
    ${STB_IMAGE_DIR}
    ${YAML_CPP_DIR}/include
) 
