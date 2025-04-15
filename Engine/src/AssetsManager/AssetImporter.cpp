#include "AssetImporter.h"


#include "../Textures/Texture.h"
#include "../Logger/Log.h"

#include "../Shaders/Shader.h"
#include "../File Loaders/glTF/glTF2Loader.h"
#include "../Materials/MaterialSerializer.h"
#include <filesystem>
#include <string>
#include <regex>

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
        GE_CORE_TRACE("AssetImporter::loadShader - Loading shader from: {}", metadata.m_filePath.string());

        const auto& vertexPath = metadata.m_filePath;
        if (!std::filesystem::exists(vertexPath)) {
            GE_CORE_ERROR("AssetImporter::loadShader - Vertex shader file not found: {}", vertexPath.string());
            return nullptr;
        }

        // Deduce fragment shader path
        // Expects naming convention like: MyShader.vert.glsl, MyShader.frag.glsl
        // Or MyShader.vs.glsl, MyShader.fs.glsl etc.
        // Handles extensions like .glsl, .shader, etc.

        std::string vertPathStr = vertexPath.string();
        std::string fragPathStr;

        // Try replacing common vertex suffixes (.vert, .vs) with fragment suffixes (.frag, .fs)
        std::regex vertRegex("(\\.vert|\\.vs)(\\..+)$"); // Matches .vert.ext or .vs.ext
        std::smatch match;

        if (std::regex_search(vertPathStr, match, vertRegex) && match.size() > 2) {
            std::string base = match.prefix().str();
            std::string ext = match[2].str();
            fragPathStr = base + (match[1].str() == ".vert" ? ".frag" : ".fs") + ext;
            GE_CORE_TRACE("AssetImporter::loadShader - Deduced fragment path: {}", fragPathStr);
        } else {
             GE_CORE_ERROR("AssetImporter::loadShader - Could not deduce fragment shader path from vertex path: {}. Expected format like 'name.vert.glsl' or 'name.vs.glsl'.", vertexPath.string());
            return nullptr;
        }

        std::filesystem::path fragmentPath(fragPathStr);

        if (!std::filesystem::exists(fragmentPath)) {
            GE_CORE_ERROR("AssetImporter::loadShader - Deduced fragment shader file not found: {}", fragmentPath.string());
            return nullptr;
        }

        // Check for an optional geometry shader using the same base name
        std::string geomPathStr;
        bool hasGeometryShader = false;
        std::filesystem::path geometryPath;

        // Try to deduce geometry shader path from vertex path
        // Look for both .gs and .geom conventions
        if (std::regex_search(vertPathStr, match, vertRegex) && match.size() > 2) {
            std::string base = match.prefix().str();
            std::string ext = match[2].str();
            
            // Try .gs first
            geomPathStr = base + ".gs" + ext;
            geometryPath = std::filesystem::path(geomPathStr);
            
            if (std::filesystem::exists(geometryPath)) {
                hasGeometryShader = true;
                GE_CORE_INFO("AssetImporter::loadShader - Found geometry shader: {}", geometryPath.string());
            } else {
                // Try .geom if .gs doesn't exist
                geomPathStr = base + ".geom" + ext;
                geometryPath = std::filesystem::path(geomPathStr);
                
                if (std::filesystem::exists(geometryPath)) {
                    hasGeometryShader = true;
                    GE_CORE_INFO("AssetImporter::loadShader - Found geometry shader: {}", geometryPath.string());
                } else {
                    GE_CORE_TRACE("AssetImporter::loadShader - No geometry shader found, using standard rendering pipeline");
                }
            }
        }

        // Create the shader using the updated Shader::create
        std::shared_ptr<Shader> shader;
        
        if (hasGeometryShader) {
            shader = Shader::create(vertexPath, fragmentPath, geometryPath);
        } else {
            shader = Shader::create(vertexPath, fragmentPath);
        }

        if (!shader || !shader->isValid()) {
            GE_CORE_ERROR("AssetImporter::loadShader - Failed to create or compile shader from {} and {}{}, with status {}", 
                vertexPath.string(), 
                fragmentPath.string(), 
                hasGeometryShader ? " and " + geometryPath.string() : "", 
                (int)shader->getStatus());
            return nullptr;
        }

        GE_CORE_INFO("AssetImporter::loadShader - Successfully loaded shader: {}", shader->getName());

        // Wrap the shader in an Asset object
        AssetVariant assetVariant = shader;
        std::shared_ptr<AssetVariant> variantPtr = std::make_shared<AssetVariant>(assetVariant);
        std::shared_ptr<Asset> asset = std::make_shared<Asset>(variantPtr);

        return asset;
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
