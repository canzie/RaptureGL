#version 430 core

// Volumetric ray marching visualization using depth-based color gradient

// Use a 2D work group size since we are generating a 2D image
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Input 3D texture (read-only) - Cascade Probe Grid
layout (binding = 0) uniform sampler3D u_InputTexture3D;

// Output 2D texture (write-only) - Visualization Target
layout (binding = 1, rgba16f) uniform restrict writeonly image2D u_OutputTexture2D;

// Uniforms for control
uniform float u_RotationAngleY = 0.0; // Rotation around Y axis in radians
uniform int u_NumSteps = 64;          // Number of steps along the ray
uniform float u_IntensityScale = 1.0; // Keep for potential future use (not used for depth color)
uniform float u_OpacityScale = 0.1;  // Scale density/opacity derived from alpha
uniform float u_OpacityThreshold = 0.05; // Threshold to consider a sample significant

// Function to create a Y-rotation matrix
mat3 rotationY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c, 0.0, s,
        0.0, 1.0, 0.0,
        -s, 0.0, c
    );
}

void main() {
    // Get the 2D invocation ID (pixel coordinates in the output 2D texture)
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 outputDims = imageSize(u_OutputTexture2D);

    // Boundary check
    if (pixelCoords.x >= outputDims.x || pixelCoords.y >= outputDims.y) {
        return;
    }

    // Calculate normalized texture coordinates for the screen pixel (-1 to 1 mapping -> 0 to 1 UV)
    vec2 screenUV = (vec2(pixelCoords) + 0.5) / vec2(outputDims);
    vec2 ndc = screenUV * 2.0 - 1.0; // Map to [-1, 1] for orthographic view

    // --- Setup Ray ---
    // Define ray origin and direction in texture coordinate space [0, 1]
    // We'll ray march along Z for an orthographic view
    vec3 rayOriginTex = vec3(screenUV.x, screenUV.y, -0.1); // Start slightly outside z=0
    vec3 rayDirTex = vec3(0.0, 0.0, 1.0);                   // March along +Z

    // Center the volume at (0.5, 0.5, 0.5) for rotation
    vec3 center = vec3(0.5);
    mat3 rotMat = rotationY(u_RotationAngleY);

    // Rotate ray origin and direction around the center
    rayOriginTex = rotMat * (rayOriginTex - center) + center;
    rayDirTex = rotMat * rayDirTex;

    // --- Ray Marching ---
    vec4 finalColor = vec4(1.0, 1.0, 1.0, 1.0); // Initialize to White (background/no hit)
    float stepSize = 1.0 / float(max(1, u_NumSteps)); // Avoid division by zero

    for (int i = 0; i < u_NumSteps; ++i) {
        // Calculate current sample position along the ray in texture coordinates
        vec3 samplePos = rayOriginTex + rayDirTex * (float(i) + 0.5) * stepSize;

        // Check if sample position is within the texture volume [0, 1]
        if (any(lessThan(samplePos, vec3(0.0))) || any(greaterThan(samplePos, vec3(1.0)))) {
            continue; // Skip samples outside the texture
        }

        // Sample the 3D probe texture
        vec4 sampledData = texture(u_InputTexture3D, samplePos);

        // Calculate opacity based on sampled alpha
        // sampledData.a = Transparency (0 = opaque, 1 = transparent)
        float opacity = (1.0 - sampledData.a) * u_OpacityScale;

        // Check if opacity exceeds threshold
        if (opacity > u_OpacityThreshold) {
            // Significant hit found! Calculate depth (0=near, 1=far)
            float depth = (float(i) + 0.5) * stepSize; // Normalized depth along the ray

            // Interpolate between Blue (near) and Red (far) based on depth
            vec3 depthColor = mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), clamp(depth, 0.0, 1.0));

            // Set the final color and exit the loop
            finalColor = vec4(depthColor, 1.0);
            break;
        }
    }

    // Write the final color (White if no hit, Blue-Red gradient if hit)
    imageStore(u_OutputTexture2D, pixelCoords, finalColor);
} 