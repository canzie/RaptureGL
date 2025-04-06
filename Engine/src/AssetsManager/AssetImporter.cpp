#include "AssetImporter.h"


#include "../Textures/Texture.h"
#include "../Logger/Log.h"

#include "../Shaders/Shader.h"
#include "../File Loaders/glTF/glTF2Loader.h"
#include "../Materials/MaterialSerializer.h"
namespace Rapture {


    bool AssetImporter::s_isInitialized = false;


    std::shared_ptr<Asset> AssetImporter::loadTexture2D(const AssetHandle& handle, const AssetMetadata& metadata){
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

    std::shared_ptr<Asset> AssetImporter::loadCubemap(const AssetHandle& handle, const AssetMetadata& metadata){
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


    std::shared_ptr<Asset> AssetImporter::loadShader(const AssetHandle& handle, const AssetMetadata& metadata){
        GE_CORE_ERROR("AssetImporter:loadShader - Not implemented");

            //std::shared_ptr<Shader> shader = Shader::create(metadata.m_filePath.string());

        return nullptr;
    }

    std::shared_ptr<Asset> AssetImporter::loadMaterial(const AssetHandle& handle, const AssetMetadata& metadata){
            
            if (metadata.m_filePath.extension() == ".gltf") {
                // Get a cached or new loader for this model file
                auto loader = ModelLoadersCache::getLoader(metadata.m_filePath.string());
                if (!loader) {
                    GE_CORE_ERROR("AssetImporter:loadMaterial - Failed to initialize glTF loader for {}", metadata.m_filePath.string());
                    return nullptr;
                }

                // Load the specific material by index
                std::shared_ptr<Material> material = loader->loadMaterialByIndex(metadata.m_indices[0]);
                if (!material) {
                    GE_CORE_ERROR("AssetImporter:loadMaterial - Failed to load material {}", metadata.m_filePath.string());
                    return nullptr;
                }

                

                AssetVariant assetVariant = material;
                std::shared_ptr<AssetVariant> variantPtr = std::make_shared<AssetVariant>(assetVariant);
                std::shared_ptr<Asset> asset = std::make_shared<Asset>(variantPtr);

                if (!asset){
                    GE_CORE_ERROR("AssetImporter:loadMaterial - Failed to instantiate material asset {}", metadata.m_filePath.string());
                    return nullptr;
                }

                return asset;
            } else if (metadata.m_filePath.extension() == ".rmat") {
                auto material = MaterialSerializer::deserialize(metadata.m_filePath);
                if (!material) {
                    GE_CORE_ERROR("AssetImporter:loadMaterial - Failed to deserialize material {}", metadata.m_filePath.string());
                    return nullptr;
                }

                AssetVariant assetVariant = material;
                std::shared_ptr<AssetVariant> variantPtr = std::make_shared<AssetVariant>(assetVariant);
                std::shared_ptr<Asset> asset = std::make_shared<Asset>(variantPtr);

                return asset;
            }

            return nullptr;
    }

    std::shared_ptr<Asset> AssetImporter::loadMesh(const AssetHandle& handle, const AssetMetadata& metadata){
        GE_CORE_ERROR("AssetImporter:loadMesh - Not implemented");
        return nullptr;
    }

    std::shared_ptr<Asset> AssetImporter::loadAnimation(const AssetHandle& handle, const AssetMetadata& metadata){
        GE_CORE_ERROR("AssetImporter:loadAnimation - Not implemented");
        return nullptr;

    }



}
