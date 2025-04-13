#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#define MAX_CASCADES 4
#define MAX_SHADOW_CASTERS 8


// Aligned for std430 layout
// Alignment rules:
// - scalar/vec2: align to size (4/8)
// - vec3/vec4/mat4: align to 16 bytes
// - uint64_t: align to 8 bytes
// - Arrays: element alignment rounded up to multiple of 16
// - Structs: largest member alignment
struct alignas(16) ShadowBufferData {
    alignas(4) int type;             // Base alignment 4
    alignas(4) uint32_t cascadeCount; // Base alignment 4
    alignas(4) uint32_t lightIndex;   // Index of the light this shadow maps to
    // uint64_t has base alignment 8, next element mat4 has 16.
    // Explicit alignment might be needed depending on packing, but often okay.
    // Array alignment will likely force padding anyway.
    alignas(8) uint64_t textureIDs[MAX_CASCADES]; // Array of 8-byte aligned uint64_t

    // Arrays of mat4 have base alignment 16. Total size = 4 * 64 = 256
    alignas(16) glm::mat4 cascadeMatrices[MAX_CASCADES];

    // Arrays of vec4 have base alignment 16. Total size = 4 * 16 = 64
    alignas(16) glm::vec4 cascadeSplitsViewSpace[MAX_CASCADES];
};


// Aligned for std430 layout
struct alignas(16) ShadowStorageLayout {
    alignas(4) uint32_t shadowCount; // Base alignment 4
    // Padding might be implicitly added here by compiler to align shadowData

    // Array of structs. Alignment is max(struct member alignment), rounded up to 16.
    // ShadowBufferData has alignas(16), so array element alignment is 16.
    alignas(16) ShadowBufferData shadowData[MAX_SHADOW_CASTERS];
};


