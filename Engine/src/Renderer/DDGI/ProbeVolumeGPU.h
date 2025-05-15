#pragma once

#include <glm/glm.hpp>


namespace Rapture {

struct ProbeConfig {
    alignas(16) glm::uvec3 probeGridDimensions = glm::uvec3(32, 8, 16); // Number of probes in each dimension (X, Y, Z)
    alignas(8) glm::uvec2 probeResolution = glm::uvec2(16, 16); // Resolution of each probe texture (e.g., 8x8)
    alignas(16) glm::vec3 probeSpacing = glm::vec3(1.0f, 1.8f, 1.5f);
    alignas(16) glm::vec3 probeOrigin = glm::vec3(0.0f, 5.0f, 0.0f); // will probably be camera position
};

struct ProbeVolume {
    alignas(16) glm::vec3 origin;

    alignas(16) glm::vec4 rotation;                           // rotation quaternion for the volume
    alignas(16) glm::vec4 probeRayRotation;                   // rotation quaternion for probe rays


    alignas(16) glm::vec3 spacing;
    alignas(16) glm::uvec3 gridDimensions;

    alignas(4) int      probeNumRays;                       // number of rays traced per probe
    alignas(4) int      probeNumIrradianceInteriorTexels;   // number of texels in one dimension of a probe's irradiance texture (does not include 1-texel border)
    alignas(4) int      probeNumDistanceInteriorTexels;     // number of texels in one dimension of a probe's distance texture (does not include 1-texel border)

    alignas(4) float    probeHysteresis;                    // weight of the previous irradiance and distance data store in probes
    alignas(4) float    probeMaxRayDistance;                // maximum world-space distance a probe ray can travel
    alignas(4) float    probeNormalBias;                    // offset along the surface normal, applied during lighting to avoid numerical instabilities when determining visibility
    alignas(4) float    probeViewBias;                      // offset along the camera view ray, applied during lighting to avoid numerical instabilities when determining visibility
    alignas(4) float    probeDistanceExponent;              // exponent used during visibility testing. High values react rapidly to depth discontinuities, but may cause banding
    alignas(4) float    probeIrradianceEncodingGamma;       // exponent that perceptually encodes irradiance for faster light-to-dark convergence

    alignas(4) float    probeBrightnessThreshold;

    // Probe Relocation, Probe Classification
    alignas(4) float    probeMinFrontfaceDistance;          // minimum world-space distance to a front facing triangle allowed before a probe is relocated

    alignas(4) float    probeRandomRayBackfaceThreshold;
    alignas(4) float    probeFixedRayBackfaceThreshold;
};

struct ProbeInfo {


};
}