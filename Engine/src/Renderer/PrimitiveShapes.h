#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Rapture {

    // Forward declarations
    class Mesh;
    class Material;

    class Line {
        public:
            Line(glm::vec3 start, glm::vec3 end, glm::vec4 color);
            
            // Getters for the Renderer to use
            inline glm::vec3 getStart() const { return m_start; }
            inline glm::vec3 getEnd() const { return m_end; }
            inline glm::vec4 getColor() const { return m_color; }
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }
            inline std::shared_ptr<Material> getMaterial() const { return m_material; }
            
        private:
            glm::vec3 m_start;
            glm::vec3 m_end;
            glm::vec4 m_color;
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material;
    };

    class Cube {
        public:
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled = false);
            
            // Getters for the Renderer to use
            inline glm::vec3 getPosition() const { return m_position; }
            inline glm::vec3 getRotation() const { return m_rotation; }
            inline glm::vec3 getScale() const { return m_scale; }
            inline glm::vec4 getColor() const { return m_color; }
            inline bool isFilled() const { return m_filled; }
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }
            inline std::shared_ptr<Material> getMaterial() const { return m_material; }
            
        private:
            glm::vec3 m_position;
            glm::vec3 m_rotation;  
            glm::vec3 m_scale;
            glm::vec4 m_color;
            bool m_filled;
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material;
    };

    class Quad {
        public:
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color);
            
            // Getters for the Renderer to use
            inline glm::vec3 getPosition() const { return m_position; }
            inline glm::vec3 getRotation() const { return m_rotation; }
            inline glm::vec3 getScale() const { return m_scale; }
            inline glm::vec4 getColor() const { return m_color; }
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }
            inline std::shared_ptr<Material> getMaterial() const { return m_material; }
            
        private:
            glm::vec3 m_position;
            glm::vec3 m_rotation;
            glm::vec3 m_scale;
            glm::vec4 m_color;
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material;
    };  

}
