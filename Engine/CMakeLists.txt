set(ENGINE_NAME Rapture)

# Include vendor libraries
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/vendor_libraries.cmake)

# Collect source files
file(GLOB_RECURSE ENGINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

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