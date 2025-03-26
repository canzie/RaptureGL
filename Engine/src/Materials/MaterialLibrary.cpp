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
std::mutex MaterialLibrary::s_mutex;

void MaterialLibrary::init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_initialized)
    {
        GE_CORE_WARN("MaterialLibrary: Already initialized!");
        return;
    }
    
    GE_CORE_INFO("MaterialLibrary: Initializing...");
    // Initialize static shaders for material types
    // PBR material shader
    if (!PBRMaterial::s_shader) {
        PBRMaterial::s_shader = new OpenGLShader("PBR_vs.glsl", "PBR_fs.glsl");
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
    
    // Specular-Glossiness material shader
    if (!SpecularGlossinessMaterial::s_shader) {
        SpecularGlossinessMaterial::s_shader = new OpenGLShader("SpecularGlossiness_vs.glsl", "SpecularGlossiness_fs.glsl");
        GE_CORE_INFO("MaterialLibrary: Initialized Specular-Glossiness shader");
    }
    
    // Create a default material
    s_defaultMaterial = std::make_shared<SolidMaterial>(glm::vec3(1.0f, 0.0f, 1.0f)); // Magenta for visibility
    s_defaultMaterial->setName("Default");
    s_materials["Default"] = s_defaultMaterial; // Direct access since we already have the lock
    
    s_initialized = true;
    GE_CORE_INFO("MaterialLibrary: Initialized successfully");
}

void MaterialLibrary::shutdown()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
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
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (hasMaterialInternal(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return s_materials[name];
    }
    
    auto material = std::make_shared<PBRMaterial>(baseColor, roughness, metallic, specular);
    material->setName(name);
    s_materials[name] = material;
    GE_CORE_INFO("MaterialLibrary: Registered material '{0}'", name);
    return material;
}

std::shared_ptr<Material> MaterialLibrary::createSolidMaterial(
    const std::string& name, 
    const glm::vec3& color)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (hasMaterialInternal(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return s_materials[name];
    }
    
    auto material = std::make_shared<SolidMaterial>(color);
    material->setName(name);
    s_materials[name] = material;
    GE_CORE_INFO("MaterialLibrary: Registered material '{0}'", name);
    return material;
}

std::shared_ptr<Material> MaterialLibrary::createPhongMaterial(
    const std::string& name,
    const glm::vec4& diffuseColor,
    const glm::vec4& specularColor,
    float shininess)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (hasMaterialInternal(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return s_materials[name];
    }
    
    auto material = std::make_shared<PhongMaterial>(1.0f, diffuseColor, specularColor, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), shininess);
    material->setName(name);
    s_materials[name] = material;
    GE_CORE_INFO("MaterialLibrary: Registered material '{0}'", name);
    return material;
}

std::shared_ptr<Material> MaterialLibrary::createSpecularGlossinessMaterial(
    const std::string& name,
    const glm::vec3& diffuseColor,
    const glm::vec3& specularColor,
    float glossiness)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (hasMaterialInternal(name))
    {
        GE_CORE_WARN("MaterialLibrary: Material with name '{0}' already exists!", name);
        return s_materials[name];
    }
    
    auto material = std::make_shared<SpecularGlossinessMaterial>(diffuseColor, specularColor, glossiness);
    material->setName(name);
    s_materials[name] = material;
    GE_CORE_INFO("MaterialLibrary: Registered material '{0}'", name);
    return material;
}

std::shared_ptr<MaterialInstance> MaterialLibrary::createMaterialInstance(
    const std::string& sourceMaterialName, 
    const std::string& instanceName)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (hasMaterialInstanceInternal(instanceName))
    {
        GE_CORE_WARN("MaterialLibrary: Material instance with name '{0}' already exists!", instanceName);
        return s_materialInstances[instanceName];
    }
    
    auto it = s_materials.find(sourceMaterialName);
    if (it == s_materials.end())
    {
        GE_CORE_ERROR("MaterialLibrary: Source material '{0}' not found!", sourceMaterialName);
        return nullptr;
    }
    
    auto sourceMaterial = it->second;
    auto instance = sourceMaterial->createInstance(instanceName);
    s_materialInstances[instanceName] = instance;
    GE_CORE_INFO("MaterialLibrary: Registered material instance '{0}'", instanceName);
    return instance;
}

std::shared_ptr<Material> MaterialLibrary::getMaterial(const std::string& name)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = s_materials.find(name);
    if (it != s_materials.end())
        return it->second;
    
    GE_CORE_WARN("MaterialLibrary: Material '{0}' not found, returning default material", name);
    return s_defaultMaterial;
}

std::shared_ptr<MaterialInstance> MaterialLibrary::getMaterialInstance(const std::string& name)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
    auto it = s_materialInstances.find(name);
    if (it != s_materialInstances.end())
        return it->second;
    
    GE_CORE_WARN("MaterialLibrary: Material instance '{0}' not found", name);
    return nullptr;
}

void MaterialLibrary::registerMaterial(const std::string& name, std::shared_ptr<Material> material)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    
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
    std::lock_guard<std::mutex> lock(s_mutex);
    
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
    std::lock_guard<std::mutex> lock(s_mutex);
    
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
    std::lock_guard<std::mutex> lock(s_mutex);
    
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
    std::lock_guard<std::mutex> lock(s_mutex);
    return hasMaterialInternal(name);
}

// Internal version without lock (to be used when mutex is already locked)
bool MaterialLibrary::hasMaterialInternal(const std::string& name)
{
    return s_materials.find(name) != s_materials.end();
}

bool MaterialLibrary::hasMaterialInstance(const std::string& name)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return hasMaterialInstanceInternal(name);
}

// Internal version without lock (to be used when mutex is already locked)
bool MaterialLibrary::hasMaterialInstanceInternal(const std::string& name)
{
    return s_materialInstances.find(name) != s_materialInstances.end();
}

std::shared_ptr<Material> MaterialLibrary::getDefaultMaterial()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_defaultMaterial;
}

} // namespace Rapture 