#include "AssetManagerEditor.h"
#include "AssetImporter.h"
#include "../Utils/UUID.h"
#include "../logger/log.h"
#include "../Materials/Material.h"

#include <filesystem>

namespace Rapture {

    AssetManagerEditor::AssetManagerEditor()
    : AssetManagerBase()
    {
        // Initialize asset registry
    }

    AssetManagerEditor::~AssetManagerEditor() {
        // Clean up resources
        for (auto& [handle, asset] : m_loadedAssets) {
            // Release any resource handles
        }
        m_loadedAssets.clear();
        m_assetRegistry.clear();
    }

    bool AssetManagerEditor::isAssetHandleValid(AssetHandle handle) const {
        return m_assetRegistry.find(handle) != m_assetRegistry.end();
    }

    bool AssetManagerEditor::isAssetLoaded(AssetHandle handle) const {
        return m_loadedAssets.find(handle) != m_loadedAssets.end() && m_loadedAssets.at(handle)->isValid();
    }

    std::shared_ptr<Asset> AssetManagerEditor::getAsset(AssetHandle handle) {
        
        if (!isAssetHandleValid(handle)) {
            GE_CORE_ERROR("AssetManager::getAsset - Invalid asset handle");
            return nullptr;
        }

        // Check if asset is already loaded
        if (isAssetLoaded(handle)) {
            return m_loadedAssets.at(handle);
        } else {
            // Import asset
            const AssetMetadata& metadata = getAssetMetadata(handle);
            auto asset = AssetImporter::importAsset(handle, metadata);
            
            // Cache the loaded asset
            if (asset) {
                m_loadedAssets.insert_or_assign(handle, asset);
                GE_CORE_INFO("AssetManager::getAsset - Asset loaded: {}", metadata.m_filePath.string());
            } else {
                GE_CORE_ERROR("AssetManager::getAsset - Failed to load asset: {}", metadata.m_filePath.string());
            }
            
            return asset;
        }
    }

    const AssetMetadata& AssetManagerEditor::getAssetMetadata(AssetHandle handle) const {
        static AssetMetadata s_nullMetadata;

        auto it = m_assetRegistry.find(handle);
        if (it != m_assetRegistry.end()) {
            return it->second;
        }
        return s_nullMetadata;
    }

    std::pair<std::shared_ptr<Asset>, AssetHandle> AssetManagerEditor::importAsset(std::filesystem::path path, std::vector<uint32_t> indices)
    {

        if (path.empty()) {
            GE_CORE_ERROR("AssetManager::importAsset - Path is empty");
            return std::make_pair(nullptr, AssetHandle());
        }

        // 1. check if the asset is already in the registry
        for (const auto& [handle, metadata] : m_assetRegistry) {
            if (metadata.m_filePath == path && metadata.m_indices == indices) {
                return std::make_pair(getAsset(handle), handle);
            }
        }


        AssetMetadata metadata;
        metadata.m_filePath = path;
        metadata.m_assetType = determineAssetType(path.string());
        metadata.m_indices = indices;

        if (metadata.m_assetType == AssetType::None) {
            GE_CORE_ERROR("AssetManager::importAsset - Unknown asset type for extension: {}", path.extension().string());
            return std::make_pair(nullptr, AssetHandle());
        }

        // generate a handle for the asset
        AssetHandle handle = UUIDGenerator::Generate();


        auto asset = AssetImporter::importAsset(handle, metadata);
        if (asset) {
            m_assetRegistry.insert_or_assign(handle, metadata);
            m_loadedAssets.insert_or_assign(handle, asset);

            return std::make_pair(asset, handle);

        } 

        GE_CORE_ERROR("AssetManager::importAsset - Failed to import asset: {}", path.string());
        return std::make_pair(nullptr, AssetHandle());


    }

    std::pair<std::shared_ptr<Asset>, AssetHandle> AssetManagerEditor::importAsset(std::vector<std::filesystem::path> paths)
    {
        for (const auto& path : paths) {
            if (path.empty()) {
                GE_CORE_ERROR("AssetManager::importAsset - Path({}) is empty", path.string());
                return std::make_pair(nullptr, AssetHandle());
            }
        }

        // 1. check if the asset is already in the registry
        for (const auto& [handle, metadata] : m_assetRegistry) {
            if (metadata.m_assetType == AssetType::Cubemap && metadata.m_cubemapPaths == paths) {
                return std::make_pair(getAsset(handle), handle);
            }
        }


        AssetMetadata metadata;
        metadata.m_cubemapPaths = paths;
        // assume all paths are of the same type
        // its fine for now, the entire system is scuffed for now, idealy we either treat every image as a seperate asset, or use some sort of atlas thing where they are all combined
        // other option is to use 1 image skyboxes, instead of a 6 image cubemap
        metadata.m_assetType = AssetType::Cubemap;

        if (metadata.m_assetType == AssetType::None) {
            GE_CORE_ERROR("AssetManager::importAsset - Unknown asset type for extension: {}", paths[0].extension().string());
            return std::make_pair(nullptr, AssetHandle());
        }

        // generate a handle for the asset
        AssetHandle handle = UUIDGenerator::Generate();


        auto asset = AssetImporter::importAsset(handle, metadata);

        m_assetRegistry.insert_or_assign(handle, metadata);
        m_loadedAssets.insert_or_assign(handle, asset);

        return std::make_pair(asset, handle);    
        
    }

    AssetType AssetManagerEditor::determineAssetType(const std::string& path) {
        std::filesystem::path filePath(path);
        std::string extension = filePath.extension().string();
        
        // Convert to lowercase for case-insensitive comparison
        for (char& c : extension) {
            c = std::tolower(c);
        }
        
        // Determine asset type based on file extension
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || 
            extension == ".tga" || extension == ".bmp" || extension == ".hdr") {
            return AssetType::Texture2D;
        }
        else if (extension == ".gltf") {
            return AssetType::Mesh;
        }
        else if (extension == ".mat") {
            return AssetType::Material;
        }
        // Add more asset types as needed
        
        GE_CORE_WARN("AssetManager::determineAssetType - Unknown asset type for extension: {}", extension);
        return AssetType::None;
    }
}
