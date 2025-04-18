#pragma once

#include <glm/glm.hpp>
#include <vector>
namespace Rapture {

/**
 * @brief Holds the configuration and GPU resource linkage for a single cascade level in the hierarchy.
 */
struct RadianceCascadeProbeGridSettings {
    /**
     * @brief The start distance t_i for the radiance interval this cascade represents.   
     */
    float rangeStart = 0.0f;

    /**
     * @brief The end distance t_{i+1} for the radiance interval.
     */
    float rangeEnd = 0.0f;

    /**
     * @brief The number of probes (spatial resolution) along each axis for this cascade's 3D grid. 
     * Corresponds to P_i scaling.
     */
    glm::ivec3 gridDimensions = glm::ivec3(0);

    /**
     * @brief A measure of the angular detail stored per probe 
     * (e.g., the dimension of an octahedral map texture, or cubemap face size). 
     * Corresponds to Q_i scaling
     */
    int angularResolution = 0;
    
    /**
     * @brief (Could be optional) The transform from world space to the local grid space of this cascade.
     */
    glm::mat4 worldToGridTransform = glm::mat4(1.0f);


    //AssetHandle probeTextureHandle

    };

struct RadianceCascadesHierarchySettings {
    /**
     * @brief The number of cascade levels in the hierarchy.
     */
    int numCascades = 0;

    /**
     * @brief The range extent of the first cascade (t_1, since t_0 is 0)
     */
    float baseRange = 0;

    /**
     * @brief The exponential factor used to determine subsequent cascade ranges 
     * (e.g., t_{i+1} = t_i * rangeScaleFactor or t_i = baseRange * pow(rangeScaleFactor, i-1)). 
     * Often 2.0
     */
    float rangeScaleFactor = 2.0f;

    /**
     * @brief The factor by which spatial resolution changes per cascade (e.g., 0.5 if dimensions halve each step)
     */
    float gridScaleFactor = 0.5f;

    /**
     * @brief The spatial grid resolution of the first cascade (P_0)
     */
    glm::ivec3 baseGridDimensions = glm::ivec3(0);


   float angularScaleFactor = 2.0f;

   std::vector<RadianceCascadeProbeGridSettings> cascadeSettings;

   // UBO/SSBO
   // cascadeInfoBufferHandle

    // populate cascadeSettings based on base parameters
    // - baseRange, rangeScaleFactor, etc
    
    void CalculateCascadeParameters() {

    };

    // creates needed textures using the params stored in cascadeSettings
    void CreateGpuResources() {

    };

    // Method to update the contents of the GPU buffer (cascadeInfoBufferHandle)
    // with the latest per-cascade data (ranges, transforms, etc.) 
    //needed by the population or sampling shaders.
    void UpdateGpuBuffers() {

    };

    
};


// struct for SSBO/UBO
struct RadianceCascadeShaderData {
/*
    vec3 gridMinWorldPos;    // World position corresponding to grid coordinate (0,0,0)
    float rangeStart;        // Start distance 't_i' for this cascade's interval
    vec3 gridCellSizeWorld;  // Size of one probe cell in world units
    float rangeEnd;          // End distance 't_{i+1}' for this cascade's interval
    ivec3 gridDimensions;    // Number of probes in X, Y, Z for this cascade
    int angularResolution;   // Target resolution for octahedral map (e.g., 8 for 8x8)
    mat4 worldToGridTransform; // Matrix to transform world pos to grid UVW coords
    mat4 inverseProjectionMatrix; // Camera inverse projection
    mat4 inverseViewMatrix;       // Camera inverse view
    vec2 screenDimensions;    // Width and height of the screen/G-Buffer
    // --- Add other necessary parameters ---
    int numRayDirections;    // Number of directions to trace per probe
    int numStepsPerRay;      // Max steps for screenspace marching
    float jitterStrength;    // Optional jitter for ray origins/directions
*/

};

} // namespace Rapture 