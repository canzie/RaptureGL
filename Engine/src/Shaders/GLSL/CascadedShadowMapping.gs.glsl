#version 420 core

// Input from vertex shader (triangles)
layout(triangles) in;

// Output as triangle strip with max 12 vertices (3 vertices * 4 cascades)
layout(triangle_strip, max_vertices = 12) out;

// Input from vertex shader
in VS_OUT {
    vec3 position;
} gs_in[];

// Shadow matrices uniform block (same as in vertex shader)
layout(std140, binding=8) uniform shadowMatrices {
    mat4 u_LightSpaceMatrix[4];
};

void main() {
    // Render to each cascade layer in a single pass
    for (int cascadeIndex = 0; cascadeIndex < 4; cascadeIndex++) {
        // Set which layer of the depth texture array we're rendering to
        gl_Layer = cascadeIndex;
        
        // For each vertex in the input triangle
        for (int i = 0; i < 3; i++) {
            // Transform the vertex using the appropriate cascade's light view-projection matrix
            gl_Position = u_LightSpaceMatrix[cascadeIndex] * vec4(gs_in[i].position, 1.0);
            
            // Pass any additional data to fragment shader if needed
            
            // Output vertex
            EmitVertex();
        }
        
        // End the primitive (triangle)
        EndPrimitive();
    }
}
