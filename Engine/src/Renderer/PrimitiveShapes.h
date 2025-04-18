#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <optional>
#include "../AssetsManager/AssetManager.h"


namespace Rapture {

    // Forward declarations
    class Mesh;
    class Material;

    struct PrimitiveConfig {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        float radius = 1.0f; // Used by Sphere
        bool isFilled = true; // Used by Cube
        bool useTexCoords = false;
        std::optional<std::string> texturePath = std::nullopt;
        AssetHandle textureHandle = 0;
        std::shared_ptr<Texture2D> texture = nullptr;
        bool createDefaultMaterial = true;
    };

    class Line {
        public:
            // NOTE: Line does not use PrimitiveConfig as it's defined by start/end points
            Line(glm::vec3 start, glm::vec3 end, glm::vec4 color);

            // Getters for the Renderer to use
            inline glm::vec3 getStart() const { return m_start; }
            inline glm::vec3 getEnd() const { return m_end; }
            inline glm::vec4 getColor() const { return m_color; }
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }
            inline std::shared_ptr<Material> getMaterial() const { return m_material; }

            // Setters for dynamic updates
            void setPoints(glm::vec3 start, glm::vec3 end);
            void setColor(const glm::vec4& color);

        private:
            glm::vec3 m_start;
            glm::vec3 m_end;
            glm::vec4 m_color;
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material;
    };

    class Cube {
        public:
            // Primary constructor
            Cube(const PrimitiveConfig& config);

            // Delegating constructors
            Cube();
            Cube(bool isCubeMap); // Special case, might need adjustment or removal? Let's keep for now.
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled = false);
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, bool useTexCoords);
            Cube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled, const std::string& texturePath);

            // Getters for the Renderer to use
            inline glm::vec3 getPosition() const { return m_position; }
            inline glm::vec3 getRotation() const { return m_rotation; }
            inline glm::vec3 getScale() const { return m_scale; }
            inline glm::vec4 getColor() const { return m_color; }
            inline bool isFilled() const { return m_filled; }
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }
            inline std::shared_ptr<Material> getMaterial() const { return m_material; }
             // Setter for material if needed externally
            inline void setMaterial(std::shared_ptr<Material> material) { m_material = material; }

        private:
            glm::vec3 m_position;
            glm::vec3 m_rotation;
            glm::vec3 m_scale;
            glm::vec4 m_color;
            bool m_filled; // Represents if the cube mesh uses filled triangles or lines
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material; // Can be nullptr if createDefaultMaterial was false

             // Private helper to initialize based on config
            void initFromConfig(const PrimitiveConfig& config);
    };

    class Quad {
        public:
            // Primary constructor
            Quad(const PrimitiveConfig& config);

            // Delegating constructors
            Quad();
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color);
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool useTexCoords);
            Quad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, const std::string& texturePath);

            // Getters for the Renderer to use
            inline glm::vec3 getPosition() const { return m_position; }
            inline glm::vec3 getRotation() const { return m_rotation; }
            inline glm::vec3 getScale() const { return m_scale; }
            inline glm::vec4 getColor() const { return m_color; }
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }
            inline std::shared_ptr<Material> getMaterial() const { return m_material; }
            // Setter for material if needed externally
            inline void setMaterial(std::shared_ptr<Material> material) { m_material = material; }

        private:
            glm::vec3 m_position;
            glm::vec3 m_rotation;
            glm::vec3 m_scale;
            glm::vec4 m_color;
            std::shared_ptr<Mesh> m_mesh;
            std::shared_ptr<Material> m_material; // Can be nullptr if createDefaultMaterial was false

            // Private helper to initialize based on config
            void initFromConfig(const PrimitiveConfig& config);
    };

    class Sphere {
        public:
            // Primary constructor
            Sphere(const PrimitiveConfig& config);

            // Delegating constructor
            Sphere(float radius);


            void setMaterial(const AssetHandle& handle); // Keep AssetHandle version? Or switch to shared_ptr? Let's keep for now.
             // Setter for material if needed externally
            inline void setMaterial(std::shared_ptr<Material> material) {
                 m_material = material;
                 // TODO: Need to decide how AssetHandle interacts here. Maybe remove m_sphereAsset?
                 // For now, setting m_material directly bypasses the AssetManager link.
                 m_sphereAsset = 0; // Invalidate handle if material is set directly?
            }

            inline std::shared_ptr<Material> getMaterial() {

                if (auto material = m_material.lock()) {
                    return material;
                } else {
                    // Fallback to AssetManager if handle is valid (legacy/alternative path)
                    if (m_sphereAsset != 0) {
                        material = AssetManager::getAsset<Material>(m_sphereAsset);
                        if (material) {
                            m_material = material;
                            return material;
                        }
                    }
                }


                return nullptr; // No material found
            }

            void initFromConfig(const PrimitiveConfig& config);
            inline std::shared_ptr<Mesh> getMesh() const { return m_mesh; }

        private:
             // Member variables set by initFromConfig
            glm::vec3 m_position;
            glm::vec3 m_rotation;
            glm::vec3 m_scale;
            glm::vec4 m_color;
            float m_radius;

            std::shared_ptr<Mesh> m_mesh;
            std::weak_ptr<Material> m_material;
            AssetHandle m_sphereAsset;
    };

} // namespace Rapture
