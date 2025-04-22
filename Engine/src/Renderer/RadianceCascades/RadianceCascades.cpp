#include "RadianceCascades.h"
#include "../../Logger/Log.h" // Assuming Log.h is accessible
#include "../../WindowContext/Application.h" // For getting camera
#include "../../Scenes/Entity.h" // For camera entity
#include "../../Scenes/Components/Components.h" // For CameraControllerComponent
#include <stdexcept> // For std::out_of_range
#include <cmath> // For std::pow
#include <string>

#include <algorithm> //

namespace Rapture {

// --- RadianceCascade Implementation ---

RadianceCascade::RadianceCascade(float start, float end, glm::ivec3 dimensions, int angularRes, glm::vec3 origin, glm::vec3 spacing) :
    m_intervalStart(start),
    m_intervalEnd(end),
    m_gridDimensions(dimensions),
    m_angularResolution(angularRes),
    m_gridOrigin(origin),
    m_probeSpacing(spacing)
{
    if (dimensions.x <= 0 || dimensions.y <= 0 || dimensions.z <= 0) {
        GE_CORE_ERROR("RadianceCascade: Invalid grid dimensions ({}, {}, {}) provided.", dimensions.x, dimensions.y, dimensions.z);
        // Handle error appropriately, maybe throw or set dimensions to 1x1x1
        m_gridDimensions = glm::max(glm::ivec3(1), dimensions); // Ensure at least 1x1x1
    }
     if (angularRes <= 0) {
        GE_CORE_ERROR("RadianceCascade: Invalid angular resolution ({}) provided.", angularRes);
        m_angularResolution = 1; // Default to minimum
    }

    // Calculate the size needed for directional data per probe (e.g., N*N for octahedral map, 6*N*N for cubemap)
    m_probeAngularDataSize = m_angularResolution * m_angularResolution;
    if (m_probeAngularDataSize <= 0) {
         GE_CORE_ERROR("RadianceCascade: Invalid probe angular data size ({}) calculated.", m_probeAngularDataSize);
         m_probeAngularDataSize = 1; // Ensure at least 1 element
    }

    uint32_t totalProbes = static_cast<uint32_t>(m_gridDimensions.x) * static_cast<uint32_t>(m_gridDimensions.y) * static_cast<uint32_t>(m_gridDimensions.z);

    // Pre-allocate and initialize probes
    m_probes.reserve(totalProbes);
    for (int z = 0; z < m_gridDimensions.z; ++z) {
        for (int y = 0; y < m_gridDimensions.y; ++y) {
            for (int x = 0; x < m_gridDimensions.x; ++x) {
                // Calculate world position of the probe center
                glm::vec3 probeCenterPos = m_gridOrigin + glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f) * m_probeSpacing;
                m_probes.emplace_back(probeCenterPos, m_probeAngularDataSize);
            }
        }
    }

    // Calculate the required dimensions for the 2D texture atlas
    glm::uvec2 res = {1, 1}; // Default to 1x1
    uint32_t probeDim = m_angularResolution; // Size of one side of the square angular data for a probe

    if (probeDim > 0 && totalProbes > 0) {
        // Arrange probes in a grid within the atlas
        // Calculate grid dimensions (in probes) to hold totalProbes, aiming for roughly square
        uint32_t probesPerRow = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(totalProbes))));
        probesPerRow = std::max(1u, probesPerRow); // Ensure at least 1 probe per row if totalProbes > 0

        uint32_t numRows = static_cast<uint32_t>(std::ceil(static_cast<float>(totalProbes) / probesPerRow));
        numRows = std::max(1u, numRows); // Ensure at least 1 row

        // Calculate final atlas dimensions in pixels
        res.x = probesPerRow * probeDim;
        res.y = numRows * probeDim;
    } else {
         GE_CORE_WARN("RadianceCascade: Cannot create atlas with zero probes ({}) or zero angular resolution ({}). Creating 1x1 atlas.", totalProbes, probeDim);
    }

    TextureSpecification spec;
    spec.width = res.x;
    spec.height = res.y;
    spec.format = TextureFormat::RGBA16F;
    m_CascadeAtlasTexture = Texture2D::create(spec);

    m_CascadeAtlasTexture->setMinFilter(TextureFilter::Linear); // Linear interpolation within a cascade is valid
    m_CascadeAtlasTexture->setMagFilter(TextureFilter::Linear);
    m_CascadeAtlasTexture->setWrapS(TextureWrap::ClampToEdge);
    m_CascadeAtlasTexture->setWrapT(TextureWrap::ClampToEdge);

     GE_CORE_TRACE("RadianceCascade: Created cascade interval [{}, {}], dimensions ({}, {}, {}), angular res {}, {} probes | Atlas Texture: ({}, {})",
                  m_intervalStart, m_intervalEnd, m_gridDimensions.x, m_gridDimensions.y, m_gridDimensions.z, m_angularResolution, totalProbes, res.x, res.y);

}







RadianceIntervalData RadianceCascade::sampleProbeData(glm::vec3 worldPos, glm::vec3 worldDir) {
    // Placeholder implementation
    // 1. Convert worldPos to grid coordinates (UVW) [0, dim-1]
    // 2. Determine which 8 probes surround the sample point
    // 3. Calculate trilinear interpolation weights based on UVW fractional part
    // 4. For each of the 8 probes:
    //    a. Convert worldDir to the probe's local angular coordinate system
    //    b. Sample the directionalData (e.g., bilinear interpolation on oct map/cubemap face)
    // 5. Interpolate the 8 sampled RadianceIntervalData values using the trilinear weights
    // 6. Return the final interpolated RadianceIntervalData

    GE_CORE_WARN("RadianceCascade::sampleProbeData - Not implemented yet.");
    return RadianceIntervalData(); // Return default value
}


// --- RadianceCascadeHierarchy Implementation ---

void RadianceCascadeHierarchy::buildCascades(const BuildParams& params) {
    m_buildParams = params; // Store params
    m_cascades.clear(); // Clear previous cascades

    if (params.numCascades <= 0) {
        GE_CORE_WARN("RadianceCascadeHierarchy::buildCascades - numCascades is zero or negative.");
        return;
    }
    if (params.rangeScaleFactor <= 1.0f) {
         GE_CORE_ERROR("RadianceCascadeHierarchy::buildCascades - rangeScaleFactor must be > 1.0.");
         m_buildParams.rangeScaleFactor = 2.0f; // Reset to default
    }
     if (params.gridScaleFactor <= 0.0f || params.gridScaleFactor >= 1.0f) {
         GE_CORE_ERROR("RadianceCascadeHierarchy::buildCascades - gridScaleFactor must be > 0.0 and < 1.0.");
         m_buildParams.gridScaleFactor = 0.5f; // Reset to default
    }
    if (params.angularScaleFactor <= 1.0f) {
         GE_CORE_ERROR("RadianceCascadeHierarchy::buildCascades - angularScaleFactor must be > 1.0.");
         m_buildParams.angularScaleFactor = 2.0f; // Reset to default
    }


    float currentRangeStart = 0.0f;
    float currentRangeEnd = params.baseRange;
    glm::vec3 currentGridDimensionsF = params.baseGridDimensions;
    float currentAngularResolutionF = static_cast<float>(params.baseAngularResolution);

    m_cascades.reserve(params.numCascades);

    // These are placeholders - they will be calculated per-cascade in UpdateGpuBuffers
    glm::vec3 placeholderOrigin(0.0f);
    glm::vec3 placeholderSpacing(1.0f);

    std::string ind = "";

    for (int i = 0; i < params.numCascades; ++i) {
        glm::ivec3 cascadeDimensions = glm::max(glm::ivec3(1), glm::ivec3(glm::round(currentGridDimensionsF)));
        int cascadeAngularRes = std::max(2, static_cast<int>(glm::round(currentAngularResolutionF))); // Min sensible angular res is 2

        m_cascades.emplace_back(currentRangeStart, currentRangeEnd, cascadeDimensions, cascadeAngularRes, placeholderOrigin, placeholderSpacing);

        // Prepare for next cascade
        currentRangeStart = currentRangeEnd;
        // Calculate next end using scale factor: t_{i+1} = t_i * scaleFactor (paper implies t_i = base * factor^(i))
        // currentRangeEnd *= params.rangeScaleFactor; // This is t_{i+1} = t_i * factor
        currentRangeEnd = params.baseRange * std::pow(params.rangeScaleFactor, i + 1); // This matches t_i ~ base * factor^i

        currentGridDimensionsF *= params.gridScaleFactor;
        currentAngularResolutionF *= params.angularScaleFactor;
    }

    GE_CORE_INFO("RadianceCascadeHierarchy::buildCascades - Built {} cascades.", m_cascades.size());
}

void RadianceCascadeHierarchy::populateCascades(/* Input data source */) {
     GE_CORE_WARN("RadianceCascadeHierarchy::populateCascades - Not implemented yet.");
     // This function would likely:
     // 1. Trigger GPU compute passes to calculate RadianceIntervalData per cascade.
     // 2. Read back the results from GPU textures/buffers into the corresponding
     //    RadianceProbe::directionalData vectors in the _cascades structure.
     //    (Requires careful mapping between GPU texture coordinates and CPU probe indices/angular data layout)
}

RadianceIntervalData RadianceCascadeHierarchy::queryRadiance(glm::vec3 worldPos, glm::vec3 worldDir) {
     GE_CORE_WARN("RadianceCascadeHierarchy::queryRadiance - Not implemented yet.");
     // Placeholder Implementation (Back-to-Front Merge):
     RadianceIntervalData accumulatedRadiance; // Starts at {0,0,0,1} (black, fully transparent)

     for (auto& cascade : m_cascades) { // Iterate 0 to N-1 (front-to-back according to paper intervals)
         RadianceIntervalData cascadeSample = cascade.sampleProbeData(worldPos, worldDir);

         // Merge using Eq. 13: La,c = La,b + βa,b * Lb,c
         // Extract RGB components for the calculation
         glm::vec3 accumulatedRgb(accumulatedRadiance.radianceAndTransparency.x, accumulatedRadiance.radianceAndTransparency.y, accumulatedRadiance.radianceAndTransparency.z);
         glm::vec3 cascadeRgb(cascadeSample.radianceAndTransparency.x, cascadeSample.radianceAndTransparency.y, cascadeSample.radianceAndTransparency.z);

         accumulatedRgb += accumulatedRadiance.radianceAndTransparency.a * cascadeRgb;

         // Update the vec4
         accumulatedRadiance.radianceAndTransparency.x = accumulatedRgb.x;
         accumulatedRadiance.radianceAndTransparency.y = accumulatedRgb.y;
         accumulatedRadiance.radianceAndTransparency.z = accumulatedRgb.z;

         // Merge transparency: βa,c = βa,b * βb,c
         accumulatedRadiance.radianceAndTransparency.a *= cascadeSample.radianceAndTransparency.a;

         // Optional: Early out if accumulatedRadiance.alpha approaches 0?
         if (accumulatedRadiance.radianceAndTransparency.a < 1e-6f) {
            // break; // Fully opaque, no need to check further cascades
         }
     }

     // After loop, accumulatedRadiance holds L0,+inf(p, ω) (or L0, tN(p,ω))
     return accumulatedRadiance;
}

} // namespace Rapture
