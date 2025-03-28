set(ENGINE_NAME Rapture)

# Include vendor libraries
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/vendor_libraries.cmake)

# Collect source files
file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Always include the profiler source files for both debug and release builds
list(APPEND ENGINE_SOURCES 
    src/Debug/Profiler.cpp
    src/Debug/GPUProfiler.cpp
    src/Debug/TracyProfiler.cpp
)

# Debug mode settings - check in multiple ways
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR RAPTURE_DEBUG)
    message(STATUS "Debug build detected - Enabling debug flags")
    add_definitions(-DRAPTURE_DEBUG)
    add_definitions(-D_DEBUG)
    add_definitions(-DRAPTURE_TRACY_PROFILING_ENABLED=1)
    add_definitions(-DTRACY_ENABLE)
elseif(MSVC)
    # For MSVC, check if we're in debug configuration
    add_compile_definitions($<$<CONFIG:Debug>:RAPTURE_DEBUG>)
    add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
    add_compile_definitions($<$<CONFIG:Debug>:RAPTURE_TRACY_PROFILING_ENABLED=1>)
    add_compile_definitions($<$<CONFIG:Debug>:TRACY_ENABLE>)
    
    message(STATUS "MSVC build - Debug flags will be enabled in Debug configuration only")
else()
    message(STATUS "Release build - Debug flags disabled")
    # Tracy can still be enabled in release mode if desired
    # add_definitions(-DRAPTURE_TRACY_PROFILING_ENABLED=1)
    # add_definitions(-DTRACY_ENABLE)
endif()

# Create engine library
add_library(${ENGINE_NAME} STATIC ${ENGINE_SOURCES})

# Include directories
target_include_directories(${ENGINE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link with vendor libraries
target_link_libraries(${ENGINE_NAME} PUBLIC
    vendor_libraries
)

# Add additional compile definitions
target_compile_definitions(${ENGINE_NAME} PRIVATE
    SPDLOG_COMPILED_LIB
    FMT_HEADER_ONLY=0
)

# Set output directory
set_target_properties(${ENGINE_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 