#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

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
            Cube();
            Cube(bool isCubeMap);
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled = false);
            // New constructor with option to use texture coordinates and normals
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, bool useTexCoords);
            // New constructor with texture path
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, const std::string& texturePath);
            
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
            Quad();
            // these values are just initial values, they should not be changed after creation
            // only execption to this is for drawing a quad for debugging purposes
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color);
            // New constructor with option to use texture coordinates and normals
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool useTexCoords);
            // New constructor with texture path
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, const std::string& texturePath);
            
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

    class Sphere {
        public:
            Sphere(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled = false);
            // New constructor with texture path
            Sphere(glm::vec4 color, const std::string& texturePath);
            Sphere(glm::vec3 scale, bool filled, const std::shared_ptr<Material>& material);

            void setMaterial(const std::shared_ptr<Material>& material);
            std::shared_ptr<Material> getMaterial() const { return m_material; }

        private:
            glm::vec3 m_position;
            glm::vec3 m_rotation;
            glm::vec3 m_scale;
            glm::vec4 m_color;
            bool m_filled;
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material;
    };

}
