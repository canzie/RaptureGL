#version 430 core

// Define the local work group size (e.g., 8x8x1 threads per group)
layout (local_size_x = 10, local_size_y = 10, local_size_z = 1) in;


layout(rgba32f, binding = 0) uniform image2D imgOutput;

uniform float time;

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
	vec2 resolution = vec2(gl_NumWorkGroups.x, gl_NumWorkGroups.y);

    vec2 uv = (texelCoord * 2.0 - resolution) / resolution.y;

    float d = length(uv);
    d = sin(d * 8.0 + time) / 8.0;
    d = abs(d);

    d = 0.02/d;

    vec4 fragColor = vec4(d, d, d, 1.0);

    imageStore(imgOutput, texelCoord, fragColor);
}

