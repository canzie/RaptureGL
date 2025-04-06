#pragma once

#include "AssetManagerEditor.h"


#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <vector>

#include "Asset.h"
#include "../Utils/UUID.h"

#include "../Logger/Log.h"


namespace Rapture {



    class AssetManager
    {
    public:

        static void init() {
            if (s_isInitialized) {
                GE_CORE_WARN("AssetManager already initialized");
                return;
            }
            s_activeAssetManager = new AssetManagerEditor();
            s_isInitialized = true;
        }

        static void shutdown() {
            if (!s_isInitialized) {
                GE_CORE_WARN("AssetManager not initialized");
                return;
            }
            delete s_activeAssetManager;
            s_isInitialized = false;
        }

        template<typename T>
        static std::shared_ptr<T> getAsset(AssetHandle handle) {
            // get the active asset manager from the "project"
            std::shared_ptr<Asset> asset = s_activeAssetManager->getAsset(handle);
            return asset->getUnderlyingAsset<T>();
        }

        template<typename T>
        static std::pair<std::shared_ptr<T>, AssetHandle> importAsset(std::filesystem::path path, std::vector<uint32_t> indices = {0}, AssetType assetType = AssetType::None) {
            auto [asset, handle] = s_activeAssetManager->importAsset(path, indices, assetType);
            return std::make_pair(asset->getUnderlyingAsset<T>(), handle);
        }

        template<typename T>
        static std::pair<std::shared_ptr<T>, AssetHandle> importAsset(std::vector<std::filesystem::path> paths) {
            auto [asset, handle] = s_activeAssetManager->importAsset(paths);
            return std::make_pair(asset->getUnderlyingAsset<T>(), handle);
        }

        // Helper method for UI to access the asset registry
        static const AssetRegistry& getAssetRegistry() {
            if (!s_isInitialized || !s_activeAssetManager) {
                GE_CORE_ERROR("AssetManager not initialized");
                static AssetRegistry emptyRegistry;
                return emptyRegistry;
            }
            return s_activeAssetManager->getAssetRegistry();
        }

        template<typename T>
        static std::pair<std::shared_ptr<T>, AssetHandle> getDefaultAsset(AssetType assetType) {
            auto [asset, handle] = s_activeAssetManager->getDefaultAsset(assetType);
            if (!asset) {
                GE_CORE_ERROR("AssetManager::getDefaultAsset - Failed to get default asset");
                return std::make_pair(nullptr, AssetHandle());
            }
            return std::make_pair(asset->getUnderlyingAsset<T>(), handle);
        }

        static const AssetMap& getLoadedAssets() {
            if (!s_isInitialized || !s_activeAssetManager) {
                GE_CORE_ERROR("AssetManager not initialized");
                static AssetMap emptyMap;
                return emptyMap;
            }
            return s_activeAssetManager->getLoadedAssets();
        }
    private:
        static bool s_isInitialized;
        static AssetManagerEditor* s_activeAssetManager;

        
        
    };


}

