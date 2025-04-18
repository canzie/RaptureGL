#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>



namespace Rapture {
namespace Entropy {

    struct SphereColliderComponent {
        glm::vec3 center;
        float radius;
    };
    
    struct CylinderColliderComponent {
        glm::vec3 start;
        glm::vec3 end;
        float radius;
    };

    struct AABBColliderComponent {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct CapsuleColliderComponent {
        glm::vec3 start;
        glm::vec3 end;
        float radius;
    };

    struct OBBColliderComponent {
        glm::vec3 center;
        glm::vec3 extents;
        glm::quat orientation;
    };

    struct ConvexHullColliderComponent {
        std::vector<glm::vec3> vertices;
    };
    
    

} // namespace Entropy
} // namespace Rapture
