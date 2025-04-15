#version 420 core

layout(triangles) in;

layout(triangle_strip, max_vertices = 12) out;

in VS_OUT {
    vec3 position;
} gs_in[];

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
            
            
            // Output vertex
            EmitVertex();
        }
        
        EndPrimitive();
    }
}
