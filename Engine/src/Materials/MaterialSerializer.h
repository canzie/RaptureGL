#pragma once

#include "Material.h"
#include "MaterialInstance.h"
#include <string>
#include <memory>

namespace Rapture {

class MaterialSerializer {
public:
    // Serialize a material to a string
    static std::string serialize(const std::shared_ptr<Material>& material);
    
    // Deserialize a material from a string
    static std::shared_ptr<Material> deserialize(const std::string& serializedMaterial);
    
    // Save a material to a file
    static bool saveToFile(const std::shared_ptr<Material>& material, const std::string& filepath);
    
    // Load a material from a file
    static std::shared_ptr<Material> loadFromFile(const std::string& filepath);
    
    // Serialize a material instance to a string
    static std::string serializeInstance(const std::shared_ptr<MaterialInstance>& materialInstance);
    
    // Deserialize a material instance from a string
    static std::shared_ptr<MaterialInstance> deserializeInstance(const std::string& serializedInstance);
    
    // Save a material instance to a file
    static bool saveInstanceToFile(const std::shared_ptr<MaterialInstance>& materialInstance, const std::string& filepath);
    
    // Load a material instance from a file
    static std::shared_ptr<MaterialInstance> loadInstanceFromFile(const std::string& filepath);
    
    // Utility methods
    static std::string materialTypeToString(MaterialType type);
    static MaterialType stringToMaterialType(const std::string& typeStr);
    
private:
    // Helper methods for serialization/deserialization
    static void serializeProperties(const std::shared_ptr<Material>& material, void* outData);
    static void deserializeProperties(std::shared_ptr<Material>& material, const void* data);
    
    static void serializeTextures(const std::shared_ptr<Material>& material, void* outData);
    static void deserializeTextures(std::shared_ptr<Material>& material, const void* data);
    
    static void serializeShaderVariants(const std::shared_ptr<Material>& material, void* outData);
    static void deserializeShaderVariants(std::shared_ptr<Material>& material, const void* data);
    
    static void serializeRenderStates(const std::shared_ptr<Material>& material, void* outData);
    static void deserializeRenderStates(std::shared_ptr<Material>& material, const void* data);
};

} // namespace Rapture 