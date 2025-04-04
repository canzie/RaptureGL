#pragma once

#include "Asset.h"

#include <memory>
#include <map>
#include <functional>
#include <variant>

#include "../Textures/Texture.h"
#include "../Logger/Log.h"
#include "../Materials/MaterialLibrary.h"

namespace Rapture {

    using AssetImporterFunction = std::function<std::shared_ptr<Asset>(AssetHandle, const AssetMetadata&)>;
    static std::map<AssetType, AssetImporterFunction> s_assetImporters;

    class AssetImporter {
    
    public:

        static void init(){
            if (s_isInitialized) {
                GE_CORE_WARN("AssetImporter already initialized");
                return;
            }
            s_assetImporters[AssetType::Texture2D] = loadTexture2D;
            s_assetImporters[AssetType::Cubemap] = loadCubemap;
            s_assetImporters[AssetType::Shader] = loadShader;
            s_assetImporters[AssetType::Material] = loadMaterial;
            s_assetImporters[AssetType::Mesh] = loadMesh;
            s_assetImporters[AssetType::Animation] = loadAnimation;
            s_isInitialized = true;
        }
        static void shutdown(){
            if (!s_isInitialized) {
                GE_CORE_WARN("AssetImporter not initialized");
                return;
            }
            s_assetImporters.clear();
            s_isInitialized = false;
        }

        static std::shared_ptr<Asset> importAsset(const AssetHandle& handle, const AssetMetadata& metadata){
            
            return s_assetImporters[metadata.m_assetType](handle, metadata);
        }

        static std::shared_ptr<Asset> loadTexture2D(const AssetHandle& handle, const AssetMetadata& metadata){
            // TODO: update the loadasync to take in a filepath handle
            // TODO: remove the overhead of storing textures in the texture library
            //       only keep a list of textures queued for loading so we dont get duplicates in the queue when spamming the load function
            std::shared_ptr<Texture2D> texture = TextureLibrary::loadAsync(metadata.m_filePath.string());

            if (!texture) {
                GE_CORE_ERROR("AssetImporter:loadTexture2D - Failed to load texture {}", metadata.m_filePath.string());
                return nullptr;
            }

            AssetVariant assetVariant = texture;
            std::shared_ptr<AssetVariant> variantPtr = std::make_shared<AssetVariant>(assetVariant);
            std::shared_ptr<Asset> asset = std::make_shared<Asset>(variantPtr);

            return asset;
        
        
        }

        static std::shared_ptr<Asset> loadCubemap(const AssetHandle& handle, const AssetMetadata& metadata){
            // TODO: update the loadasync to take in a filepath handle
            // TODO: remove the overhead of storing textures in the texture library
            //       only keep a list of textures queued for loading so we dont get duplicates in the queue when spamming the load function
            std::shared_ptr<Texture2D> texture = TextureLibrary::loadCubemap(metadata.m_cubemapPaths);

            if (!texture) {
                GE_CORE_ERROR("AssetImporter:loadCubemap - Failed to load cubemap {}", metadata.m_cubemapPaths[0].string());
                return nullptr;
            }

            AssetVariant assetVariant = texture;
            std::shared_ptr<AssetVariant> variantPtr = std::make_shared<AssetVariant>(assetVariant);
            std::shared_ptr<Asset> asset = std::make_shared<Asset>(variantPtr);

            return asset;
        
        
        }


        static std::shared_ptr<Asset> loadShader(const AssetHandle& handle, const AssetMetadata& metadata){
            GE_CORE_ERROR("AssetImporter:loadShader - Not implemented");
            return nullptr;
        }

        static std::shared_ptr<Asset> loadMaterial(const AssetHandle& handle, const AssetMetadata& metadata){



            GE_CORE_ERROR("AssetImporter:loadMaterial - Not implemented");
            return nullptr;
        }

        static std::shared_ptr<Asset> loadMesh(const AssetHandle& handle, const AssetMetadata& metadata){
            GE_CORE_ERROR("AssetImporter:loadMesh - Not implemented");
            return nullptr;
        }

        static std::shared_ptr<Asset> loadAnimation(const AssetHandle& handle, const AssetMetadata& metadata){
            GE_CORE_ERROR("AssetImporter:loadAnimation - Not implemented");
            return nullptr;

        }

    private:
        static bool s_isInitialized;
        
        
        
        

    };

}


// for loading meshes, we can load each primitive as an asset if loaded trough a non-static mesh
// -> need a way to save multiple meshes belonging to the same gltf file, same for the materials
// 
// what is a model asset?

