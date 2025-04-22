#pragma once

#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <memory> // For shared_ptr
#include <limits> // Required for std::numeric_limits
#include <algorithm> // Needed for std::max used below

#include "../../Textures/Texture.h" 


// Forward declarations
class RadianceCascade;
class RadianceProbe;
struct RadianceIntervalData;
class Texture3D; // Forward declare Texture3D
class ShaderStorageBuffer; // Forward declare SSBO
class Shader; // Forward declare Shader

namespace Rapture {

// Stores the configuration settings for building the cascade hierarchy.
// Analogous to RadianceCascadesHierarchySettings from the previous attempt.
struct BuildParams {
    /**
     * @brief The number of cascade levels in the hierarchy.
     */
    int numCascades = 6;

    /**
     * @brief The range extent of the first cascade (t_1, since t_0 is 0)
     */
    float baseRange = 2.0f;

    /**
     * @brief The exponential factor used to determine subsequent cascade ranges (t_i ~ pow(rangeScaleFactor, i)).
     * Must be > 1. Typically 2.0.
     */
    float rangeScaleFactor = 2.0f;

    /**
     * @brief The factor by which spatial resolution changes per cascade (e.g., 0.5 means dimensions halve each step).
     * This affects probe spacing (Δp ~ 1 / gridScaleFactor^i). Should be < 1.
     * Based on paper scaling Δp ~ 2^i, if t_i ~ 2^i, this implies probe spacing doubles, so grid dimensions should halve.
     */
    float gridScaleFactor = 0.5f;

    /**
     * @brief The spatial grid resolution of the first cascade (P_0)
     */
    glm::ivec3 baseGridDimensions = glm::ivec3(32, 16, 32); // Example dimensions

    /**
     * @brief The angular resolution 'dimension' of the first cascade (Q_0).
     * E.g., for an NxN octahedral map, this would be N.
     */
    int baseAngularResolution = 8; // Example angular resolution

   /**
    * @brief The factor by which angular resolution changes per cascade.
    * Based on paper scaling Δω ~ 1/2^i, angular resolution should double.
    * Must be > 1. Typically 2.0.
    */
   float angularScaleFactor = 2.0f;

   // Screen dimensions needed for calculating frustum slices
   //glm::ivec2 screenDimensions = glm::ivec2(1024, 1024); // Default, should be updated

   // --- Parameters for GPU population (placeholders for now) ---
   int numRayDirections = 16; // Example
   int numStepsPerRay = 128;  // Example
   float jitterStrength = 0.0f; // Example
};


// Stores the core data for a single direction within a probe for a specific interval [a, b].
struct RadianceIntervalData {
    alignas(16) glm::vec4 radianceAndTransparency = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
};

// Represents a single probe at a specific location within a cascade's grid.
struct RadianceProbe {
    glm::vec3 position; // World space position of the probe center

    std::vector<RadianceIntervalData> directionalData;

    RadianceProbe(const glm::vec3& pos, int angularDataSize) : position(pos) {
        directionalData.resize(angularDataSize);
    }
};

// Manages a single cascade, corresponding to a specific radiance interval [ti, ti+1].
class RadianceCascade {

public:
    RadianceCascade(float start, float end, glm::ivec3 dimensions, int angularRes, glm::vec3 origin, glm::vec3 spacing);
    ~RadianceCascade() = default; // Default destructor is likely fine

    // Getters
    float getIntervalStart() const { return m_intervalStart; }
    float getIntervalEnd() const { return m_intervalEnd; }
    const glm::ivec3& getGridDimensions() const { return m_gridDimensions; }
    const glm::vec3& getGridOrigin() const { return m_gridOrigin; }
    const glm::vec3& getProbeSpacing() const { return m_probeSpacing; }
    int getAngularResolution() const { return m_angularResolution; }
    int getProbeAngularDataSize() const { return m_probeAngularDataSize; }

    std::shared_ptr<Texture2D> getAtlasTexture() const { return m_CascadeAtlasTexture; }

    // Total number of probes in this cascade
    size_t getProbeCount() const { return m_probes.size(); }

    // Handles spatial interpolation between probes and angular sampling within a probe.
    // Placeholder: Actual implementation depends heavily on chosen angular representation and GPU strategy.
    RadianceIntervalData sampleProbeData(glm::vec3 worldPos, glm::vec3 worldDir);

    // Placeholder: Implement shift/extend logic if needed for specific population strategies (Sec 2.3.3)
    // void shift(glm::vec3 direction, float distance);
    // void extend(float factor);


private:
    float m_intervalStart; // ti
    float m_intervalEnd; // ti+1
    glm::ivec3 m_gridDimensions; // number of probes in each dimension (Px, Py, Pz)
    int m_angularResolution; // N 
    int m_probeAngularDataSize; // Total number of RadianceIntervalData per probe (e.g., N*N)

    // These define the grid in world space, calculated dynamically based on camera frustum slice
    glm::vec3 m_gridOrigin; // World-space origin of the probe grid (min corner of AABB)
    glm::vec3 m_probeSpacing; // World-space distance between probe centers (AABB size / gridDimensions)

    // Probe storage
    std::vector<RadianceProbe> m_probes; // Flattened 3D grid (index = z*dimX*dimY + y*dimX + x)

    std::shared_ptr<Texture2D> m_CascadeAtlasTexture;

    // Helper to convert 3D index to 1D index
};

// Manages the entire set of cascades. Holds the CPU-side representation.
class RadianceCascadeHierarchy {
public:
    RadianceCascadeHierarchy() = default;
    ~RadianceCascadeHierarchy() = default;

    // Builds the cascade structure based on the provided parameters.
    // Clears any existing cascades.
    void buildCascades(const BuildParams& params);

    // Fills probe data. Placeholder - likely involves reading data back from GPU.
    void populateCascades(/* Input data source, e.g., GPU readback buffer */);

    // Core query function. Iterates through cascades, samples each, and merges them using Eq. 13.
    // Placeholder - actual implementation details depend on strategy.
    RadianceIntervalData queryRadiance(glm::vec3 worldPos, glm::vec3 worldDir);

    // Helper to merge a range of cascades. Placeholder.
    // RadianceCascade mergeCascades(int startCascade, int endCascade);

    // Getters
    const std::vector<RadianceCascade>& getCascades() const { return m_cascades; }
    std::vector<RadianceCascade>& getCascades() { return m_cascades; }
    size_t getNumCascades() const { return m_cascades.size(); }
    const BuildParams& getBuildParams() const { return m_buildParams; }

private:
    std::vector<RadianceCascade> m_cascades; // Ordered list of cascades 0..N-1
    BuildParams m_buildParams; // Store the parameters used to build this hierarchy

};

// Structure matching the SSBO layout for sending cascade data to the GPU
// Designed for std430 layout (common for SSBOs)
struct RadianceCascadeShaderData {
    // --- Cascade Grid Definition (Derived from Frustum Slice AABB) ---
    alignas(16) glm::vec3 gridMinWorldPos;      // Min corner of the cascade's AABB in world space
    alignas(16) glm::vec3 gridCellSizeWorld;    // Size of one probe cell in world space
    alignas(16) glm::ivec3 gridDimensions;       // Number of probes (Px, Py, Pz)
    alignas(16) glm::mat4 worldToGridTransform; // Optional: Transform world pos to [0,1] grid UVW (might not be needed for SSR)

    alignas(4)  float rangeStart;             // Near plane distance (t_i) for this cascade
    alignas(4)  float rangeEnd;               // Far plane distance (t_{i+1}) for this cascade
    alignas(4)  int angularResolution;        // Resolution N for the NxN octahedral map per probe

    // --- Atlas Information ---
    alignas(8) glm::ivec2 atlasProbeGridDim;
    alignas(8) glm::ivec2 atlasPixelDim;

    // --- Camera Information (Needed for SSR) ---
    alignas(16) glm::mat4 inverseProjectionMatrix;
    alignas(16) glm::mat4 inverseViewMatrix;
    alignas(16) glm::mat4 projectionMatrix;       // Added for world-to-screen projection
    alignas(16) glm::mat4 viewMatrix;             // Added for world-to-screen projection
    alignas(16) glm::vec3 cameraWorldPos;       // vec3 aligns to 16 bytes in std430

    // --- Screen & Ray Parameters ---
    alignas(4) int numRayDirections;         // Potentially useful meta-data, but oct-map defines directions
    alignas(4) int numStepsPerRay;         // Steps for ray marching
    alignas(4) float jitterStrength;         // Jitter amount for ray marching steps

    //alignas(8) uint64_t atlasTextureHandle;

    // --- Manual Padding to match std430 size (480 bytes) ---
    // Struct data SHOULD end at offset 468. Compiler reports sizeof=464. Add 16 bytes padding.

};

struct RadianceCascadeShaderData2 {
    // --- Cascade Definition ---
    alignas(8) glm::ivec2 gridDimensionsScreen;     // Number of probes in screen space (Px, Py) for this cascade
    alignas(4) int cascadeIndex;             // Index of this cascade (0 to N-1)
    alignas(4) int angularResolution;        // Resolution N for the NxN octahedral map per probe

    alignas(4) float rangeStart;             // Near depth range (t_i) for this cascade (world units)
    alignas(4) float rangeEnd;               // Far depth range (t_{i+1}) for this cascade (world units)

    // --- Atlas Information (Where to write output) ---
    alignas(8) glm::ivec2 atlasProbeGridDim;      // Layout of probes within the atlas texture (Grid W, Grid H in probes)
    alignas(8) glm::ivec2 atlasPixelDim;          // Total size of the atlas texture in pixels (W, H)

    // --- Camera Information (Needed for screen-space to world/view space conversions during SSR) ---
    alignas(16) glm::mat4 inverseProjectionMatrix;
    alignas(16) glm::mat4 inverseViewMatrix;
    // Note: projectionMatrix and viewMatrix might be derivable from inverses if needed, or passed if frequently used. Removed for brevity.
    // alignas(16) glm::mat4 projectionMatrix;
    // alignas(16) glm::mat4 viewMatrix;
    alignas(16) glm::vec3 cameraWorldPos;       // Camera position in world space (vec3 aligns to 16 bytes in std430)

    // --- Screen & Ray Parameters ---
    alignas(8) glm::ivec2 screenDimensions;       // Full resolution of the screen/G-Buffer
    alignas(4) int numStepsPerRay;         // Steps for screen-space ray marching
    alignas(4) float jitterStrength;         // Jitter amount for ray marching steps

    // --- Padding --- Calculated for std430 alignment (total size should be multiple of 16 bytes)
    // Current members occupy 208 bytes. This is a multiple of 16. No padding needed.

}; // Expected size: 208 bytes

} // namespace Rapture
