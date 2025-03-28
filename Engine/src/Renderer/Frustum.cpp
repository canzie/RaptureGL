#include "Frustum.h"
#include "../Scenes/Components/BoundingBox.h"
#include "../Logger/Log.h"

namespace Rapture
{
    void Frustum::update(const glm::mat4& projection, const glm::mat4& view)
    {
        // Compute the view-projection matrix
        glm::mat4 viewProj = projection * view;

        // Extract frustum planes from the view-projection matrix
        // Left plane
        _planes[0].x = viewProj[0][3] + viewProj[0][0];
        _planes[0].y = viewProj[1][3] + viewProj[1][0];
        _planes[0].z = viewProj[2][3] + viewProj[2][0];
        _planes[0].w = viewProj[3][3] + viewProj[3][0];

        // Right plane
        _planes[1].x = viewProj[0][3] - viewProj[0][0];
        _planes[1].y = viewProj[1][3] - viewProj[1][0];
        _planes[1].z = viewProj[2][3] - viewProj[2][0];
        _planes[1].w = viewProj[3][3] - viewProj[3][0];

        // Bottom plane
        _planes[2].x = viewProj[0][3] + viewProj[0][1];
        _planes[2].y = viewProj[1][3] + viewProj[1][1];
        _planes[2].z = viewProj[2][3] + viewProj[2][1];
        _planes[2].w = viewProj[3][3] + viewProj[3][1];

        // Top plane
        _planes[3].x = viewProj[0][3] - viewProj[0][1];
        _planes[3].y = viewProj[1][3] - viewProj[1][1];
        _planes[3].z = viewProj[2][3] - viewProj[2][1];
        _planes[3].w = viewProj[3][3] - viewProj[3][1];

        // Near plane - Fixed to avoid culling objects close to the camera
        // Instead of adding the z component, we're taking just the pure z component
        // This creates a more accurate near plane
        _planes[4].x = viewProj[0][2];
        _planes[4].y = viewProj[1][2];
        _planes[4].z = viewProj[2][2];
        _planes[4].w = viewProj[3][2];

        // Far plane
        _planes[5].x = viewProj[0][3] - viewProj[0][2];
        _planes[5].y = viewProj[1][3] - viewProj[1][2];
        _planes[5].z = viewProj[2][3] - viewProj[2][2];
        _planes[5].w = viewProj[3][3] - viewProj[3][2];

        // Normalize all planes
        for (auto& plane : _planes)
        {
            float length = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
            if (length > 0.0001f)  // Avoid division by near-zero
            {
                plane /= length;
            }
            else
            {
                GE_RENDER_WARN("Frustum plane normalization failed: near-zero length");
            }
        }
    }

    FrustumResult Frustum::testBoundingBox(const BoundingBox& boundingBox) const
    {
        // Early exit for invalid bounding boxes
        if (!boundingBox.isValid())
        {
            return FrustumResult::Outside;
        }

        // Get the corners of the bounding box
        glm::vec3 min = boundingBox.getMin();
        glm::vec3 max = boundingBox.getMax();

        // Flag to track if the box is fully inside
        bool fullyInside = true;

        // Test against each frustum plane
        for (int i = 0; i < _planes.size(); i++)
        {
            const auto& plane = _planes[i];
            
            // Find the point that is furthest along the normal direction (positive vertex)
            glm::vec3 positiveVertex;
            positiveVertex.x = (plane.x > 0.0f) ? max.x : min.x;
            positiveVertex.y = (plane.y > 0.0f) ? max.y : min.y;
            positiveVertex.z = (plane.z > 0.0f) ? max.z : min.z;

            // Find the point that is furthest along the inverse normal direction (negative vertex)
            glm::vec3 negativeVertex;
            negativeVertex.x = (plane.x > 0.0f) ? min.x : max.x;
            negativeVertex.y = (plane.y > 0.0f) ? min.y : max.y;
            negativeVertex.z = (plane.z > 0.0f) ? min.z : max.z;

            // Calculate distance
            float negDistance = plane.x * negativeVertex.x + plane.y * negativeVertex.y + 
                                plane.z * negativeVertex.z + plane.w;
            
            // Apply a small bias/epsilon for the near plane (index 4)
            // This prevents objects close to the camera from being culled incorrectly
            if (i == 4) { // Near plane index
                constexpr float NEAR_PLANE_EPSILON = 0.05f; // Adjust as needed
                negDistance += NEAR_PLANE_EPSILON;
            }
            
            // If the negative vertex is outside, the box is outside
            if (negDistance < 0.0f)
            {
                return FrustumResult::Outside;
            }

            // Calculate positive vertex distance for inside/intersect determination
            float posDistance = plane.x * positiveVertex.x + plane.y * positiveVertex.y + 
                                plane.z * positiveVertex.z + plane.w;
                                
            // If the positive vertex is outside, the box is intersecting
            if (posDistance < 0.0f)
            {
                fullyInside = false;
            }
        }

        return fullyInside ? FrustumResult::Inside : FrustumResult::Intersect;
    }
} 