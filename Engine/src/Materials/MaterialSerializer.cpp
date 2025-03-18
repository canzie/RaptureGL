#include "MaterialSerializer.h"
#include "../Logger/Log.h"
#include "MaterialLibrary.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <yaml-cpp/yaml.h>

namespace Rapture {

// Helper functions for serialization
namespace {
    std::string materialTypeToString(MaterialType type) {
        switch (type) {
            case MaterialType::PBR: return "PBR";
            case MaterialType::PHONG: return "PHONG";
            case MaterialType::SOLID: return "SOLID";
            case MaterialType::CUSTOM: return "CUSTOM";
            default: return "UNKNOWN";
        }
    }
    
    MaterialType stringToMaterialType(const std::string& typeStr) {
        if (typeStr == "PBR") return MaterialType::PBR;
        if (typeStr == "PHONG") return MaterialType::PHONG;
        if (typeStr == "SOLID") return MaterialType::SOLID;
        if (typeStr == "CUSTOM") return MaterialType::CUSTOM;
        return MaterialType::CUSTOM;
    }
    
    std::string parameterTypeToString(MaterialParameterType type) {
        switch (type) {
            case MaterialParameterType::FLOAT: return "FLOAT";
            case MaterialParameterType::INT: return "INT";
            case MaterialParameterType::BOOL: return "BOOL";
            case MaterialParameterType::VEC2: return "VEC2";
            case MaterialParameterType::VEC3: return "VEC3";
            case MaterialParameterType::VEC4: return "VEC4";
            case MaterialParameterType::MAT3: return "MAT3";
            case MaterialParameterType::MAT4: return "MAT4";
            case MaterialParameterType::TEXTURE2D: return "TEXTURE2D";
            case MaterialParameterType::TEXTURECUBE: return "TEXTURECUBE";
            default: return "UNKNOWN";
        }
    }
    
    MaterialParameterType stringToParameterType(const std::string& typeStr) {
        if (typeStr == "FLOAT") return MaterialParameterType::FLOAT;
        if (typeStr == "INT") return MaterialParameterType::INT;
        if (typeStr == "BOOL") return MaterialParameterType::BOOL;
        if (typeStr == "VEC2") return MaterialParameterType::VEC2;
        if (typeStr == "VEC3") return MaterialParameterType::VEC3;
        if (typeStr == "VEC4") return MaterialParameterType::VEC4;
        if (typeStr == "MAT3") return MaterialParameterType::MAT3;
        if (typeStr == "MAT4") return MaterialParameterType::MAT4;
        if (typeStr == "TEXTURE2D") return MaterialParameterType::TEXTURE2D;
        if (typeStr == "TEXTURECUBE") return MaterialParameterType::TEXTURECUBE;
        return MaterialParameterType::NONE;
    }
}

std::string MaterialSerializer::serialize(const std::shared_ptr<Material>& material)
{
    if (!material) {
        GE_CORE_ERROR("MaterialSerializer::serialize: Cannot serialize null material");
        return "";
    }
    
    std::stringstream ss;
    ss << "Material:" << std::endl;
    ss << "Name=" << material->getName() << std::endl;
    ss << "Type=" << materialTypeToString(material->getType()) << std::endl;
    
    // TODO: Serialize flags and other properties
    
    // For now, let's just serialize a few common parameters
    if (material->hasParameter("baseColor")) {
        const auto& param = material->getParameter("baseColor");
        const auto& value = param.asVec3();
        ss << "Parameter:baseColor=VEC3:" << value.x << "," << value.y << "," << value.z << std::endl;
    }
    
    if (material->hasParameter("color")) {
        const auto& param = material->getParameter("color");
        const auto& value = param.asVec3();
        ss << "Parameter:color=VEC3:" << value.x << "," << value.y << "," << value.z << std::endl;
    }
    
    if (material->hasParameter("roughness")) {
        ss << "Parameter:roughness=FLOAT:" << material->getParameter("roughness").asFloat() << std::endl;
    }
    
    if (material->hasParameter("metallic")) {
        ss << "Parameter:metallic=FLOAT:" << material->getParameter("metallic").asFloat() << std::endl;
    }
    
    return ss.str();
}

std::shared_ptr<Material> MaterialSerializer::deserialize(const std::string& serializedMaterial)
{
    std::stringstream ss(serializedMaterial);
    std::string line;
    std::string name;
    MaterialType type = MaterialType::CUSTOM;
    
    // Parse header
    std::getline(ss, line);
    if (line != "Material:") {
        GE_CORE_ERROR("MaterialSerializer::deserialize: Invalid material format");
        return nullptr;
    }
    
    // Parse properties
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        
        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) continue;
        
        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);
        
        if (key == "Name") {
            name = value;
        }
        else if (key == "Type") {
            type = stringToMaterialType(value);
        }
        else if (key.find("Parameter:") == 0) {
            // We'll handle parameters when we create the material
        }
    }
    
    // Create the material based on type
    std::shared_ptr<Material> material;
    switch (type) {
        case MaterialType::PBR:
            material = MaterialLibrary::createPBRMaterial(name);
            break;
        case MaterialType::PHONG:
            material = MaterialLibrary::createPhongMaterial(name);
            break;
        case MaterialType::SOLID:
            material = MaterialLibrary::createSolidMaterial(name);
            break;
        default:
            GE_CORE_ERROR("MaterialSerializer::deserialize: Unsupported material type");
            return nullptr;
    }
    
    // Now parse parameters and apply them
    ss.clear();
    ss.seekg(0);
    
    // Skip header
    std::getline(ss, line);
    
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        
        if (line.find("Parameter:") == 0) {
            size_t equalsPos = line.find('=');
            if (equalsPos == std::string::npos) continue;
            
            std::string paramName = line.substr(10, equalsPos - 10);
            std::string paramValueTyped = line.substr(equalsPos + 1);
            
            size_t colonPos = paramValueTyped.find(':');
            if (colonPos == std::string::npos) continue;
            
            std::string paramTypeStr = paramValueTyped.substr(0, colonPos);
            std::string paramValueStr = paramValueTyped.substr(colonPos + 1);
            
            MaterialParameterType paramType = stringToParameterType(paramTypeStr);
            
            // Parse and set parameter based on type
            switch (paramType) {
                case MaterialParameterType::FLOAT: {
                    float value = std::stof(paramValueStr);
                    material->setFloat(paramName, value);
                    break;
                }
                case MaterialParameterType::INT: {
                    int value = std::stoi(paramValueStr);
                    material->setInt(paramName, value);
                    break;
                }
                case MaterialParameterType::BOOL: {
                    bool value = (paramValueStr == "true" || paramValueStr == "1");
                    material->setBool(paramName, value);
                    break;
                }
                case MaterialParameterType::VEC3: {
                    float x, y, z;
                    sscanf(paramValueStr.c_str(), "%f,%f,%f", &x, &y, &z);
                    material->setVec3(paramName, glm::vec3(x, y, z));
                    break;
                }
                // TODO: Add other parameter types as needed
                default:
                    GE_CORE_WARN("MaterialSerializer::deserialize: Unsupported parameter type {0}", paramTypeStr);
                    break;
            }
        }
    }
    
    return material;
}

bool MaterialSerializer::saveToFile(const std::shared_ptr<Material>& material, const std::string& filepath)
{
    if (!material)
    {
        GE_CORE_ERROR("MaterialSerializer: Cannot save null material to file!");
        return false;
    }

    try
    {
        YAML::Node rootNode;
        
        // Basic material properties
        rootNode["name"] = material->getName();
        rootNode["type"] = materialTypeToString(material->getType());
        rootNode["shader"] = "Standard"; // TODO: Store actual shader name
        
        // Material properties
        YAML::Node propertiesNode;
        
        // Common properties based on material type
        if (material->getType() == MaterialType::PBR)
        {
            if (material->hasParameter("baseColor"))
            {
                auto baseColor = material->getParameter("baseColor").asVec3();
                propertiesNode["baseColor"] = YAML::Node(YAML::NodeType::Sequence);
                propertiesNode["baseColor"].push_back(baseColor.x);
                propertiesNode["baseColor"].push_back(baseColor.y);
                propertiesNode["baseColor"].push_back(baseColor.z);
            }
            
            if (material->hasParameter("roughness"))
                propertiesNode["roughness"] = material->getParameter("roughness").asFloat();
                
            if (material->hasParameter("metallic"))
                propertiesNode["metallic"] = material->getParameter("metallic").asFloat();
                
            if (material->hasParameter("specular"))
                propertiesNode["specular"] = material->getParameter("specular").asFloat();
        }
        else if (material->getType() == MaterialType::PHONG)
        {
            if (material->hasParameter("diffuseColor"))
            {
                auto diffuseColor = material->getParameter("diffuseColor").asVec4();
                propertiesNode["diffuseColor"] = YAML::Node(YAML::NodeType::Sequence);
                propertiesNode["diffuseColor"].push_back(diffuseColor.x);
                propertiesNode["diffuseColor"].push_back(diffuseColor.y);
                propertiesNode["diffuseColor"].push_back(diffuseColor.z);
                propertiesNode["diffuseColor"].push_back(diffuseColor.w);
            }
            
            if (material->hasParameter("specularColor"))
            {
                auto specularColor = material->getParameter("specularColor").asVec4();
                propertiesNode["specularColor"] = YAML::Node(YAML::NodeType::Sequence);
                propertiesNode["specularColor"].push_back(specularColor.x);
                propertiesNode["specularColor"].push_back(specularColor.y);
                propertiesNode["specularColor"].push_back(specularColor.z);
                propertiesNode["specularColor"].push_back(specularColor.w);
            }
            
            if (material->hasParameter("shininess"))
                propertiesNode["shininess"] = material->getParameter("shininess").asFloat();
        }
        else if (material->getType() == MaterialType::SOLID)
        {
            if (material->hasParameter("color"))
            {
                auto color = material->getParameter("color").asVec3();
                propertiesNode["color"] = YAML::Node(YAML::NodeType::Sequence);
                propertiesNode["color"].push_back(color.x);
                propertiesNode["color"].push_back(color.y);
                propertiesNode["color"].push_back(color.z);
            }
        }
        
        rootNode["properties"] = propertiesNode;
        
        // Default render states
        YAML::Node renderStatesNode;
        renderStatesNode["cullMode"] = "Back";
        renderStatesNode["depthTest"] = true;
        renderStatesNode["depthWrite"] = true;
        renderStatesNode["blendMode"] = "Opaque";
        
        rootNode["renderStates"] = renderStatesNode;
        
        // Write to file
        std::ofstream fout(filepath);
        if (!fout.is_open())
        {
            GE_CORE_ERROR("MaterialSerializer: Failed to open file '{0}' for writing!", filepath);
            return false;
        }
        
        // Add a header comment
        fout << "# Rapture Engine Material File" << std::endl;
        fout << YAML::Dump(rootNode);
        fout.close();
        
        GE_CORE_INFO("MaterialSerializer: Successfully saved material '{0}' to file '{1}'", 
                material->getName(), filepath);
        return true;
    }
    catch (const std::exception& e)
    {
        GE_CORE_ERROR("MaterialSerializer: Failed to save material to file '{0}': {1}", 
                 filepath, e.what());
        return false;
    }
}

std::shared_ptr<Material> MaterialSerializer::loadFromFile(const std::string& filepath)
{
    try
    {
        YAML::Node rootNode = YAML::LoadFile(filepath);
        
        if (!rootNode["name"] || !rootNode["type"])
        {
            GE_CORE_ERROR("MaterialSerializer: Invalid material file format! Missing required fields.");
            return nullptr;
        }
        
        std::string name = rootNode["name"].as<std::string>();
        std::string typeStr = rootNode["type"].as<std::string>();
        MaterialType type = stringToMaterialType(typeStr);
        
        std::shared_ptr<Material> material = nullptr;
        
        // Create the appropriate material type
        if (type == MaterialType::PBR)
        {
            glm::vec3 baseColor(0.5f);
            float roughness = 0.5f;
            float metallic = 0.0f;
            float specular = 0.5f;
            
            if (rootNode["properties"])
            {
                auto props = rootNode["properties"];
                
                if (props["baseColor"] && props["baseColor"].IsSequence() && props["baseColor"].size() >= 3)
                {
                    baseColor.x = props["baseColor"][0].as<float>();
                    baseColor.y = props["baseColor"][1].as<float>();
                    baseColor.z = props["baseColor"][2].as<float>();
                }
                
                if (props["roughness"])
                    roughness = props["roughness"].as<float>();
                
                if (props["metallic"])
                    metallic = props["metallic"].as<float>();
                
                if (props["specular"])
                    specular = props["specular"].as<float>();
            }
            
            material = MaterialLibrary::createPBRMaterial(name, baseColor, roughness, metallic, specular);
        }
        else if (type == MaterialType::PHONG)
        {
            glm::vec4 diffuseColor(0.5f, 0.5f, 0.5f, 1.0f);
            glm::vec4 specularColor(1.0f, 1.0f, 1.0f, 1.0f);
            float shininess = 32.0f;
            
            if (rootNode["properties"])
            {
                auto props = rootNode["properties"];
                
                if (props["diffuseColor"] && props["diffuseColor"].IsSequence() && props["diffuseColor"].size() >= 4)
                {
                    diffuseColor.x = props["diffuseColor"][0].as<float>();
                    diffuseColor.y = props["diffuseColor"][1].as<float>();
                    diffuseColor.z = props["diffuseColor"][2].as<float>();
                    diffuseColor.w = props["diffuseColor"][3].as<float>();
                }
                
                if (props["specularColor"] && props["specularColor"].IsSequence() && props["specularColor"].size() >= 4)
                {
                    specularColor.x = props["specularColor"][0].as<float>();
                    specularColor.y = props["specularColor"][1].as<float>();
                    specularColor.z = props["specularColor"][2].as<float>();
                    specularColor.w = props["specularColor"][3].as<float>();
                }
                
                if (props["shininess"])
                    shininess = props["shininess"].as<float>();
            }
            
            material = MaterialLibrary::createPhongMaterial(name, diffuseColor, specularColor, shininess);
        }
        else if (type == MaterialType::SOLID)
        {
            glm::vec3 color(1.0f);
            
            if (rootNode["properties"])
            {
                auto props = rootNode["properties"];
                
                if (props["color"] && props["color"].IsSequence() && props["color"].size() >= 3)
                {
                    color.x = props["color"][0].as<float>();
                    color.y = props["color"][1].as<float>();
                    color.z = props["color"][2].as<float>();
                }
            }
            
            material = MaterialLibrary::createSolidMaterial(name, color);
        }
        else
        {
            GE_CORE_ERROR("MaterialSerializer: Unsupported material type '{0}'", typeStr);
            return nullptr;
        }
        
        if (!material)
        {
            GE_CORE_ERROR("MaterialSerializer: Failed to create material from file '{0}'", filepath);
            return nullptr;
        }
        
        // Apply render states if specified
        if (rootNode["renderStates"])
        {
            auto rs = rootNode["renderStates"];
            
            if (rs["cullMode"])
            {
                std::string cullMode = rs["cullMode"].as<std::string>();
                // TODO: Apply cull mode
            }
            
            if (rs["depthTest"])
            {
                bool depthTest = rs["depthTest"].as<bool>();
                // TODO: Apply depth test
            }
            
            if (rs["depthWrite"])
            {
                bool depthWrite = rs["depthWrite"].as<bool>();
                // TODO: Apply depth write
            }
            
            if (rs["blendMode"])
            {
                std::string blendMode = rs["blendMode"].as<std::string>();
                // TODO: Apply blend mode
                
                if (blendMode != "Opaque")
                {
                    material->setFlag(MaterialFlagBitLocations::TRANSPARENT, true);
                }
            }
        }
        
        GE_CORE_INFO("MaterialSerializer: Successfully loaded material '{0}' from file '{1}'",
                name, filepath);
        return material;
    }
    catch (const std::exception& e)
    {
        GE_CORE_ERROR("MaterialSerializer: Failed to load material from file '{0}': {1}", 
                 filepath, e.what());
        return nullptr;
    }
}

std::string MaterialSerializer::serializeInstance(const std::shared_ptr<MaterialInstance>& materialInstance)
{
    // For now, a simple implementation
    if (!materialInstance) {
        GE_CORE_ERROR("MaterialSerializer::serializeInstance: Cannot serialize null material instance");
        return "";
    }
    
    std::stringstream ss;
    ss << "MaterialInstance:" << std::endl;
    ss << "Name=" << materialInstance->getName() << std::endl;
    ss << "BaseMaterial=" << materialInstance->getBaseMaterial()->getName() << std::endl;
    
    // TODO: Serialize material instance-specific parameters
    
    return ss.str();
}

std::shared_ptr<MaterialInstance> MaterialSerializer::deserializeInstance(const std::string& serializedInstance)
{
    // For now, a simple implementation
    std::stringstream ss(serializedInstance);
    std::string line;
    std::string name;
    std::string baseMaterialName;
    
    // Parse header
    std::getline(ss, line);
    if (line != "MaterialInstance:") {
        GE_CORE_ERROR("MaterialSerializer::deserializeInstance: Invalid material instance format");
        return nullptr;
    }
    
    // Parse properties
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        
        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) continue;
        
        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);
        
        if (key == "Name") {
            name = value;
        }
        else if (key == "BaseMaterial") {
            baseMaterialName = value;
        }
    }
    
    // Create the material instance
    return MaterialLibrary::createMaterialInstance(baseMaterialName, name);
}

bool MaterialSerializer::saveInstanceToFile(const std::shared_ptr<MaterialInstance>& materialInstance, const std::string& filepath)
{
    std::string serialized = serializeInstance(materialInstance);
    if (serialized.empty()) {
        return false;
    }
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        GE_CORE_ERROR("MaterialSerializer::saveInstanceToFile: Failed to open file '{0}' for writing", filepath);
        return false;
    }
    
    file << serialized;
    file.close();
    
    GE_CORE_INFO("Saved material instance '{0}' to file '{1}'", materialInstance->getName(), filepath);
    return true;
}

std::shared_ptr<MaterialInstance> MaterialSerializer::loadInstanceFromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        GE_CORE_ERROR("MaterialSerializer::loadInstanceFromFile: Failed to open file '{0}' for reading", filepath);
        return nullptr;
    }
    
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    
    std::shared_ptr<MaterialInstance> instance = deserializeInstance(ss.str());
    if (instance) {
        GE_CORE_INFO("Loaded material instance '{0}' from file '{1}'", instance->getName(), filepath);
    }
    
    return instance;
}

} // namespace Rapture 