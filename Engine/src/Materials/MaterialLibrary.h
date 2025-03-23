#pragma once

#include "Material.h"
#include "MaterialInstance.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace Rapture {

class MaterialLibrary {
public:
    // Initialize the material library
    static void init();
    static void shutdown();

    // Material creation
    static std::shared_ptr<Material> createPBRMaterial(const std::string& name, 
                                                      const glm::vec3& baseColor = glm::vec3(0.5f),
                                                      float roughness = 0.5f, 
                                                      float metallic = 0.0f, 
                                                      float specular = 0.5f);
    
    static std::shared_ptr<Material> createPhongMaterial(const std::string& name, 
                                                        const glm::vec4& diffuseColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
                                                        const glm::vec4& specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                                        float shininess = 32.0f);
    
    static std::shared_ptr<Material> createSolidMaterial(const std::string& name, 
                                                        const glm::vec3& color = glm::vec3(1.0f));
    
    static std::shared_ptr<Material> createSpecularGlossinessMaterial(const std::string& name,
                                                                     const glm::vec3& diffuseColor = glm::vec3(0.8f, 0.8f, 0.8f),
                                                                     const glm::vec3& specularColor = glm::vec3(1.0f, 1.0f, 1.0f),
                                                                     float glossiness = 0.5f);

    // Material instance creation
    static std::shared_ptr<MaterialInstance> createMaterialInstance(const std::string& sourceMaterialName, 
                                                                  const std::string& instanceName);
    
    // Material retrieval
    static std::shared_ptr<Material> getMaterial(const std::string& name);
    static std::shared_ptr<MaterialInstance> getMaterialInstance(const std::string& name);
    
    // Material registration
    static void registerMaterial(const std::string& name, std::shared_ptr<Material> material);
    static void registerMaterialInstance(const std::string& name, std::shared_ptr<MaterialInstance> instance);
    
    // Material existence check
    static bool hasMaterial(const std::string& name);
    static bool hasMaterialInstance(const std::string& name);
    
    // Default materials
    static std::shared_ptr<Material> getDefaultMaterial();

    // Remove materials
    static void removeMaterial(const std::string& name);
    static void removeMaterialInstance(const std::string& name);

private:
    static std::unordered_map<std::string, std::shared_ptr<Material>> s_materials;
    static std::unordered_map<std::string, std::shared_ptr<MaterialInstance>> s_materialInstances;
    static std::shared_ptr<Material> s_defaultMaterial;
    static bool s_initialized;
};

} // namespace Rapture 