#version 460

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Input texture array
layout(binding = 0) uniform sampler2DArray inputTexArray;

// Output flattened texture
layout(binding = 1, rgba32f) uniform image2D outputTex;

// Push constants for dimensions
uniform int layerCount;      // Number of layers in the texture array
uniform int layerWidth;      // Width of each layer
uniform int layerHeight;     // Height of each layer
uniform int tilesPerRow;     // Number of tiles to arrange horizontally

void main() {
    // Get the current pixel coordinates
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    
    // Calculate which tile (layer) this pixel belongs to
    int tileX = pixelCoord.x / layerWidth;
    int tileY = pixelCoord.y / layerHeight;
    int layerIndex = tileY * tilesPerRow + tileX;
    
    // If we're outside the valid layers, return early
    if (layerIndex >= layerCount) {
        return;
    }
    
    // Calculate the local coordinates within the tile
    ivec2 localCoord;
    localCoord.x = pixelCoord.x % layerWidth;
    localCoord.y = pixelCoord.y % layerHeight;
    
    // Sample from the texture array
    vec3 sampledColor = texelFetch(inputTexArray, ivec3(localCoord, layerIndex), 0).xyz;
    
    // Write to the output texture
    imageStore(outputTex, pixelCoord, vec4(sampledColor, 1.0));
}
