#pragma once

#include "../Mesh/Mesh.h"
#include "../Textures/Texture.h"
#include "../Utils/UUID.h"

#include <filesystem>
#include <cstdint>
#include <variant>
#include <string>

namespace Rapture {

    // Forward declarations to break circular dependency
    class Material;
    class Shader; // Forward declare Shader

    using AssetHandle = UUID;
    // NOTE: i dont like this but dont know variants well enough and dont want to change the entire codebase
    using AssetVariant = std::variant<std::monostate, std::shared_ptr<Mesh>, std::shared_ptr<Texture2D>, std::shared_ptr<Material>, std::shared_ptr<Shader>>;

    enum class AssetType {
        None = 0,
        Mesh,
        Texture2D,
        Cubemap,
        Material,
        Skeleton,
        Animation,
        Audio,
        Script,
        Scene,
        Font,
        Shader,
        
    };

    inline std::string AssetTypeToString(AssetType type) {
        switch (type) {
            case AssetType::None: return "None";
            case AssetType::Mesh: return "Mesh";
            case AssetType::Texture2D: return "Texture2D";
            case AssetType::Cubemap: return "Cubemap";
            case AssetType::Material: return "Material";
            case AssetType::Skeleton: return "Skeleton";
            case AssetType::Animation: return "Animation";
            case AssetType::Audio: return "Audio";
            case AssetType::Script: return "Script";
            case AssetType::Scene: return "Scene";
            case AssetType::Font: return "Font";
            case AssetType::Shader: return "Shader";
            default: return "Unknown";
        }
    }

    struct AssetMetadata {

        AssetType m_assetType = AssetType::None;
        std::filesystem::path m_filePath;
        // some assets might be in the same file, the indices should point to them
        // indices will be mostly 1 element, but in case of loading multiple primitives in 1 static mesh
        // the indices will indicate which ones
        std::vector<uint32_t> m_indices;

        // NOTE: Very scuffed, so think about this when the assets manager works
        std::vector<std::filesystem::path> m_cubemapPaths;

        operator bool() const {
            return m_assetType != AssetType::None;
        }

    };


    class Asset {
    public:
        Asset(std::shared_ptr<AssetVariant> asset) : m_asset(asset) { }

        template<typename T>
        std::shared_ptr<T> getUnderlyingAsset() const {
            if (std::holds_alternative<std::shared_ptr<T>>(*m_asset)) {
                return std::get<std::shared_ptr<T>>(*m_asset);
            }
            return nullptr;
        }

        bool isValid() const {
            return !std::holds_alternative<std::monostate>(*m_asset);
        }


    public:
        AssetHandle m_handle;
        //AssetType getAssetType() const;

    private:
        std::shared_ptr<AssetVariant> m_asset;
    
    
    };
}
