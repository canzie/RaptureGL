cmake_minimum_required(VERSION 3.16)
project(LiDAR_Game VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
add_subdirectory(Engine)
add_subdirectory(Editor) 