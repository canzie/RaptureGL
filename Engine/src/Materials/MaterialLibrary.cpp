#include "MaterialLibrary.h"
#include "MaterialInstance.h"
#include "../Shaders/OpenGLShaders/OpenGLShader.h"
#include "../Logger/Log.h"

namespace Rapture {

// Static member initialization
std::unordered_map<std::string, std::shared_ptr<Material>> MaterialLibrary::s_materials;
std::unordered_map<std::string, std::shared_ptr<MaterialInstance>> MaterialLibrary::s_materialInstances;
std::shared_ptr<Material> MaterialLibrary::s_defaultMaterial;
bool MaterialLibrary::s_initialized = false;

void MaterialLibrary::init()
{
    if (s_initialized)
    {
        GE_CORE_WARN("MaterialLibrary: Already initialized!");
        return;
    }
    
    GE_CORE_INFO("MaterialLibrary: Initializing...");
    // Initialize static shaders for material types
    // Metal material shader
    if (!MetalMaterial::s_shader) {
        MetalMaterial::s_shader = new OpenGLShader("PBR_vs.glsl", "PBR_fs.glsl");
        GE_CORE_INFO("MaterialLibrary: Initialized PBR shader");
    }
    
    // Phong material shader
    if (!PhongMaterial::s_shader) {
        PhongMaterial::s_shader = new OpenGLShader("blinn_phong_vs.glsl", "blinn_phong_fs.glsl");
        GE_CORE_INFO("MaterialLibrary: Initialized Phong shader");
    }
    
    // Solid material shader
    if (!SolidMaterial::s_shader) {
        SolidMaterial::s_shader = new OpenGLShader("default_vs.glsl", "default_fs.glsl");
        GE_CORE_INFO("MaterialLibrary: Initialized Solid shader");
    }
    
    // Create a default material
    s_defaultMaterial = std::make_shared<SolidMaterial>(glm::vec3(1.0f, 0.0f, 1.0f)); // Magenta for visibility
    registerMaterial("Default", s_defaultMaterial);
    
    s_initialized = true;
    GE_CORE_INFO("MaterialLibrary: Initialized successfully");
}

void MaterialLibrary::shutdown()
{
    if (!s_initialized)
    {
        GE_CORE_WARN("MaterialLibrary: Not initialized, nothing to shut down!");
        return;
    }
    
    GE_CORE_INFO("MaterialLibrary: Shutting down...");
    
    s_materials.clear();
    s_materialInstances.clear();
    s_defaultMaterial = nullptr;
    
    s_initialized = false;
    GE_CORE_INFO("MaterialLibrary: Shut down successfully");
}

std::shared_ptr<Material> MaterialLibrary::createPBRMaterial(
    const std::string& name, 
    const glm::vec3& baseColor, 
    float roughness, 
    float metallic, 
    float specular)
{
    if (hasMaterial(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return getMaterial(name);
    }
    
    auto material = std::make_shared<MetalMaterial>(baseColor, roughness, metallic, specular);
    registerMaterial(name, material);
    return material;
}

std::shared_ptr<Material> MaterialLibrary::createSolidMaterial(
    const std::string& name, 
    const glm::vec3& color)
{
    if (hasMaterial(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return getMaterial(name);
    }
    
    auto material = std::make_shared<SolidMaterial>(color);
    registerMaterial(name, material);
    return material;
}

std::shared_ptr<Material> MaterialLibrary::createPhongMaterial(
    const std::string& name,
    const glm::vec4& diffuseColor,
    const glm::vec4& specularColor,
    float shininess)
{
    if (hasMaterial(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return getMaterial(name);
    }
    
    auto material = std::make_shared<PhongMaterial>(1.0f, diffuseColor, specularColor, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), shininess);
    registerMaterial(name, material);
    return material;
}

std::shared_ptr<MaterialInstance> MaterialLibrary::createMaterialInstance(
    const std::string& sourceMaterialName, 
    const std::string& instanceName)
{
    if (hasMaterialInstance(instanceName))
    {
        GE_CORE_WARN("MaterialLibrary: Material instance with name '{0}' already exists!", instanceName);
        return getMaterialInstance(instanceName);
    }
    
    auto sourceMaterial = getMaterial(sourceMaterialName);
    if (!sourceMaterial)
    {
        GE_CORE_ERROR("MaterialLibrary: Source material '{0}' not found!", sourceMaterialName);
        return nullptr;
    }
    
    auto instance = sourceMaterial->createInstance(instanceName);
    registerMaterialInstance(instanceName, instance);
    return instance;
}

std::shared_ptr<Material> MaterialLibrary::getMaterial(const std::string& name)
{
    auto it = s_materials.find(name);
    if (it != s_materials.end())
        return it->second;
    
    GE_CORE_WARN("MaterialLibrary: Material '{0}' not found, returning default material", name);
    return s_defaultMaterial;
}

std::shared_ptr<MaterialInstance> MaterialLibrary::getMaterialInstance(const std::string& name)
{
    auto it = s_materialInstances.find(name);
    if (it != s_materialInstances.end())
        return it->second;
    
    GE_CORE_WARN("MaterialLibrary: Material instance '{0}' not found", name);
    return nullptr;
}

void MaterialLibrary::registerMaterial(const std::string& name, std::shared_ptr<Material> material)
{
    if (!material)
    {
        GE_CORE_ERROR("MaterialLibrary: Cannot register null material!");
        return;
    }
    
    s_materials[name] = material;
    GE_CORE_INFO("MaterialLibrary: Registered material '{0}'", name);
}

void MaterialLibrary::registerMaterialInstance(const std::string& name, std::shared_ptr<MaterialInstance> instance)
{
    if (!instance)
    {
        GE_CORE_ERROR("MaterialLibrary: Cannot register null material instance!");
        return;
    }
    
    s_materialInstances[name] = instance;
    GE_CORE_INFO("MaterialLibrary: Registered material instance '{0}'", name);
}

void MaterialLibrary::removeMaterial(const std::string& name)
{
    auto it = s_materials.find(name);
    if (it != s_materials.end())
    {
        s_materials.erase(it);
        GE_CORE_INFO("MaterialLibrary: Removed material '{0}'", name);
    }
    else
    {
        GE_CORE_WARN("MaterialLibrary: Cannot remove material '{0}', not found", name);
    }
}

void MaterialLibrary::removeMaterialInstance(const std::string& name)
{
    auto it = s_materialInstances.find(name);
    if (it != s_materialInstances.end())
    {
        s_materialInstances.erase(it);
        GE_CORE_INFO("MaterialLibrary: Removed material instance '{0}'", name);
    }
    else
    {
        GE_CORE_WARN("MaterialLibrary: Cannot remove material instance '{0}', not found", name);
    }
}

bool MaterialLibrary::hasMaterial(const std::string& name)
{
    return s_materials.find(name) != s_materials.end();
}

bool MaterialLibrary::hasMaterialInstance(const std::string& name)
{
    return s_materialInstances.find(name) != s_materialInstances.end();
}

std::shared_ptr<Material> MaterialLibrary::getDefaultMaterial()
{
    return s_defaultMaterial;
}

} // namespace Rapture 