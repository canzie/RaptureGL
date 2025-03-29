cmake_minimum_required(VERSION 3.16)
project(LiDAR_Game VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable parallel compilation
if(MSVC)
    add_compile_options(/MP)
else()
    include(ProcessorCount)
    ProcessorCount(N)
    if(NOT N EQUAL 0)
        set(CMAKE_BUILD_PARALLEL_LEVEL ${N})
    endif()
endif()

# Add subdirectories
add_subdirectory(Engine)
add_subdirectory(Editor) 