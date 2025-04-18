#version 430 core

// Define the local work group size (e.g., 8x8x1 threads per group)
layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;


layout(rgba32f, binding = 0) uniform image2D imgOutput;

uniform float time; // Add time uniform back

// Constants for the sphere
const vec3 sphereCenter = vec3(0.0, 0.0, -1.0);
const float sphereRadius = 0.5;

// Basic camera setup
const vec3 cameraOrigin = vec3(0.0, 0.0, 0.0);
const vec3 viewLowerLeft = vec3(-1.0, -1.0, -1.0); // Lower-left corner of the view plane
const vec3 viewHorizontal = vec3(2.0, 0.0, 0.0);   // Width vector of the view plane
const vec3 viewVertical = vec3(0.0, 2.0, 0.0);     // Height vector of the view plane

// Background color
const vec4 backgroundColor = vec4(0.2, 0.3, 0.7, 1.0); // A nice blue
// Sphere color
const vec4 sphereColor = vec4(1.0, 0.5, 0.2, 1.0);   // Orangey

// Light properties
// const vec3 lightPosition = vec3(2.0, 2.0, 0.0); // Remove constant light position
const vec4 lightColor = vec4(1.0, 1.0, 1.0, 1.0); // White light
const float ambientStrength = 0.1;
const float environmentInfluence = 0.2; // How much the background color influences the sphere

// Ray-Sphere intersection function
// Returns true if the ray intersects the sphere, false otherwise.
// 't' will contain the distance along the ray to the closest intersection point if hit.
bool hitSphere(const vec3 rayOrigin, const vec3 rayDir, float tMin, float tMax, out float t) {
    vec3 oc = rayOrigin - sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = dot(oc, rayDir);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - a * c;

    if (discriminant > 0.0) {
        float sqrtDiscriminant = sqrt(discriminant);
        float temp = (-b - sqrtDiscriminant) / a;
        if (temp < tMax && temp > tMin) {
            t = temp;
            return true;
        }
        temp = (-b + sqrtDiscriminant) / a;
        if (temp < tMax && temp > tMin) {
            t = temp;
            return true;
        }
    }
    return false;
}


void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dims = imageSize(imgOutput); // Get the dimensions of the output image

    // Calculate UV coordinates in [0, 1] range
    float u = float(texelCoord.x) / float(dims.x);
    float v = float(texelCoord.y) / float(dims.y);

    // Calculate ray direction
    vec3 rayDirection = normalize(viewLowerLeft + u * viewHorizontal + v * viewVertical - cameraOrigin);
    vec3 rayOrigin = cameraOrigin;

    float t; // Intersection distance
    vec4 pixelColor = backgroundColor; // Default to background color

    // Check for intersection with the sphere
    if (hitSphere(rayOrigin, rayDirection, 0.001, 1000.0, t)) {
        vec3 hitPoint = rayOrigin + rayDirection * t;
        vec3 N = normalize(hitPoint - sphereCenter); // Normal at the hit point

        // Calculate dynamic light position orbiting the sphere
        float orbitRadius = 2.0;
        float orbitSpeed = 1.0;
        vec3 currentLightPosition = sphereCenter + vec3(orbitRadius * cos(time * orbitSpeed), 2.0, orbitRadius * sin(time * orbitSpeed));

        vec3 lightDir = normalize(currentLightPosition - hitPoint);

        // Simple diffuse lighting (Lambertian)
        float diff = max(dot(N, lightDir), 0.0); // Clamp to >= 0
        vec4 diffuseColor = diff * lightColor;

        // Combine ambient and diffuse
        vec4 ambientColor = ambientStrength * lightColor;
        pixelColor = (ambientColor + diffuseColor) * sphereColor;
        pixelColor.a = 1.0; // Ensure alpha is 1

        // Blend with background color for environment influence
        pixelColor = mix(pixelColor, backgroundColor, environmentInfluence);
    }

    imageStore(imgOutput, texelCoord, pixelColor);
}

