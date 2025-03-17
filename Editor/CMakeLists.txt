# Editor CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Set the Editor project name
set(EDITOR_NAME LiDAR_Editor)

# Find all source files in the Editor directory
file(GLOB_RECURSE EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

# Create Editor executable with appropriate entry point
add_executable(${EDITOR_NAME} ${EDITOR_SOURCES})

# Set include directories
target_include_directories(${EDITOR_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Engine/src  # Include Engine headers
)

# Ensure C++20 standard
set_property(TARGET ${EDITOR_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${EDITOR_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Link with Engine library and vendor libraries directly
target_link_libraries(${EDITOR_NAME} PRIVATE 
    Rapture
    spdlog  # Link directly with spdlog
)

# Set output directory
set_target_properties(${EDITOR_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Fix MSVC warning about conflicting libraries
if(MSVC)
    # Avoid default library conflicts
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(${EDITOR_NAME} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:MSVCRT")
    endif()
endif()

# Copy necessary assets to output directory
add_custom_command(TARGET ${EDITOR_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${EDITOR_NAME}>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${EDITOR_NAME}>/assets
    COMMENT "Copying editor assets to build directory"
)

# Copy runtime dependencies (DLLs, shaders, etc.) to output directory
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/Engine/vendor/GLFW/bin/glfw3.dll")
    add_custom_command(TARGET ${EDITOR_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Engine/vendor/GLFW/bin/glfw3.dll"
        $<TARGET_FILE_DIR:${EDITOR_NAME}>
    )
endif()

# IDE source organization
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${EDITOR_SOURCES}) 