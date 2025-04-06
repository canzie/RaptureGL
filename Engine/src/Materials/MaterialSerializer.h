#pragma once

#include "Material.h"
#include <string>
#include <memory>
#include <filesystem>

namespace Rapture {

class MaterialSerializer {
public:
    static std::shared_ptr<Material> deserialize(const std::filesystem::path& filepath);
};

} // namespace Rapture 

