#version 420 core

// Empty fragment shader - we only care about depth output
// The depth value is automatically written by OpenGL from gl_Position

void main() {
    // No additional processing needed for depth-only rendering
    // The depth is written automatically from the rasterization stage
}
