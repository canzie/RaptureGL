#include "MaterialSerializer.h"
#include "MaterialLibrary.h"
#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>
#include <filesystem>

#include "../Logger/Log.h"

namespace Rapture {

std::shared_ptr<Material> MaterialSerializer::deserialize(const std::filesystem::path& filepath) {
    try {
        YAML::Node materialNode = YAML::LoadFile(filepath.string());
        
        if (!materialNode["name"] || !materialNode["type"] || !materialNode["color"]) {
            return nullptr;
        }

        std::string name = materialNode["name"].as<std::string>();
        std::string type = materialNode["type"].as<std::string>();
        
        if (type == "SOLID") {
            auto colorNode = materialNode["color"];
            glm::vec3 color(
                colorNode["r"].as<float>(),
                colorNode["g"].as<float>(),
                colorNode["b"].as<float>()
            );
            
            return MaterialLibrary::createSolidMaterial(name, color);
        }
        
        return nullptr;
    }
    catch (const YAML::Exception& e) {
        GE_CORE_ERROR("MaterialSerializer::deserialize - Error deserializing material: {}", e.what());
        return nullptr;
    }
}

} // namespace Rapture
