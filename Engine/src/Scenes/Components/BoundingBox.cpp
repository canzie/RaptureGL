#include "BoundingBox.h"
#include "../../Logger/Log.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Rapture {

    void BoundingBox::reset() {
        _min = glm::vec3(std::numeric_limits<float>::max());
        _max = glm::vec3(std::numeric_limits<float>::lowest());
        _isValid = false;
    }

    BoundingBox BoundingBox::calculateFromVertices(const std::vector<float>& vertices, size_t stride, size_t offset) {
        if (vertices.empty() || stride < 3) {
            return BoundingBox(); // Return invalid bounding box
        }

        glm::vec3 min(std::numeric_limits<float>::max());
        glm::vec3 max(std::numeric_limits<float>::lowest());
        
        // Iterate through each vertex
        for (size_t i = offset; i < vertices.size(); i += stride) {
            // Make sure we don't go out of bounds
            if (i + 2 >= vertices.size()) {
                break;
            }
            
            float x = vertices[i];
            float y = vertices[i + 1];
            float z = vertices[i + 2];
            
            // Update min and max
            min.x = std::min(min.x, x);
            min.y = std::min(min.y, y);
            min.z = std::min(min.z, z);
            
            max.x = std::max(max.x, x);
            max.y = std::max(max.y, y);
            max.z = std::max(max.z, z);
        }
        
        return BoundingBox(min, max);
    }

    BoundingBox BoundingBox::transform(const glm::mat4& matrix) const {
        if (!_isValid) {
            return BoundingBox(); // Return invalid bounding box
        }
        
        // Transform all 8 corners of the bounding box
        glm::vec3 corners[8];
        corners[0] = glm::vec3(_min.x, _min.y, _min.z);
        corners[1] = glm::vec3(_min.x, _min.y, _max.z);
        corners[2] = glm::vec3(_min.x, _max.y, _min.z);
        corners[3] = glm::vec3(_min.x, _max.y, _max.z);
        corners[4] = glm::vec3(_max.x, _min.y, _min.z);
        corners[5] = glm::vec3(_max.x, _min.y, _max.z);
        corners[6] = glm::vec3(_max.x, _max.y, _min.z);
        corners[7] = glm::vec3(_max.x, _max.y, _max.z);
        
        // Initialize new bounds
        glm::vec3 newMin(std::numeric_limits<float>::max());
        glm::vec3 newMax(std::numeric_limits<float>::lowest());
        
        // Transform each corner and update bounds
        for (int i = 0; i < 8; ++i) {
            glm::vec4 transformedCorner = matrix * glm::vec4(corners[i], 1.0f);
            transformedCorner /= transformedCorner.w; // Perspective division
            
            glm::vec3 transformedVec3(transformedCorner);
            
            newMin = glm::min(newMin, transformedVec3);
            newMax = glm::max(newMax, transformedVec3);
        }
        
        return BoundingBox(newMin, newMax);
    }

    BoundingBox BoundingBox::merge(const BoundingBox& other) const {
        // If either box is invalid, return the other one
        if (!_isValid && !other.isValid()) {
            return BoundingBox(); // Both invalid, return invalid
        }
        
        if (!_isValid) {
            return other;
        }
        
        if (!other.isValid()) {
            return *this;
        }
        
        // Both valid, calculate merged bounds
        glm::vec3 newMin = glm::min(_min, other.getMin());
        glm::vec3 newMax = glm::max(_max, other.getMax());
        
        return BoundingBox(newMin, newMax);
    }

    bool BoundingBox::contains(const glm::vec3& point) const {
        if (!_isValid) {
            return false;
        }
        
        return point.x >= _min.x && point.x <= _max.x &&
               point.y >= _min.y && point.y <= _max.y &&
               point.z >= _min.z && point.z <= _max.z;
    }

    bool BoundingBox::intersects(const BoundingBox& other) const {
        if (!_isValid || !other.isValid()) {
            return false;
        }
        
        // Check for overlap in all three dimensions
        return (_min.x <= other.getMax().x && _max.x >= other.getMin().x) &&
               (_min.y <= other.getMax().y && _max.y >= other.getMin().y) &&
               (_min.z <= other.getMax().z && _max.z >= other.getMin().z);
    }

    void BoundingBox::logBounds() const {
        if (_isValid) {
            GE_CORE_INFO("BoundingBox: Min({:.2f}, {:.2f}, {:.2f}), Max({:.2f}, {:.2f}, {:.2f})", 
                _min.x, _min.y, _min.z, _max.x, _max.y, _max.z);
        } else {
            GE_CORE_WARN("BoundingBox: Invalid");
        }
    }

} // namespace Rapture 