#include "AssetImporter.h"


#include "../Textures/Texture.h"
#include "../Logger/Log.h"

#include "../Shaders/Shader.h"
#include "../File Loaders/glTF/glTF2Loader.h"
#include "../Materials/MaterialSerializer.h"
#include <filesystem>
#include <string>
#include <regex>
#include <optional>
#include <map>
#include <vector>

namespace Rapture {

// Helper function to find related shader file paths
std::optional<std::filesystem::path> getRelatedShaderPath(
    const std::filesystem::path& basePath,
    const std::string& targetStage) 
{
    if (!std::filesystem::exists(basePath)) {
        GE_CORE_WARN("AssetImporter::getRelatedShaderPath - Base path does not exist: {}", basePath.string());
        return std::nullopt;
    }

    std::string basePathStr = basePath.string();
    // Regex to capture: (base_name)(.stage)(.extension)
    // Example: "path/to/MyShader.vert.glsl" -> ("path/to/MyShader")(".vert")(".glsl")
    std::regex pathRegex("^(.*?)(\\.(?:vert|vs|frag|fs|geom|gs|comp|cs))(\\.[^.]+)$");
    std::smatch match;

    if (!std::regex_match(basePathStr, match, pathRegex) || match.size() != 4) {
        GE_CORE_WARN("AssetImporter::getRelatedShaderPath - Could not parse base shader path structure: {}. Expected format like 'name.stage.ext'.", basePathStr);
        return std::nullopt;
    }

    std::string baseName = match[1].str();
    std::string finalExt = match[3].str();

    const std::map<std::string, std::array<std::string, 2>> stageExtensions = {
        {"vertex", {".vert", ".vs"}},
        {"fragment", {".frag", ".fs"}},
        {"geometry", {".geom", ".gs"}},
        {"compute", {".comp", ".cs"}}
    };

    if (stageExtensions.find(targetStage) == stageExtensions.end()) {
        GE_CORE_ERROR("AssetImporter::getRelatedShaderPath - Invalid target shader stage requested: {}", targetStage);
        return std::nullopt;
    }

    for (const auto& ext : stageExtensions.at(targetStage)) {
        std::filesystem::path potentialPath = baseName + ext + finalExt;
        if (std::filesystem::exists(potentialPath)) {
            GE_CORE_TRACE("AssetImporter::getRelatedShaderPath - Found related {} shader: {}", targetStage, potentialPath.string());
            return potentialPath;
        }
    }

    GE_CORE_TRACE("AssetImporter::getRelatedShaderPath - Could not find related {} shader for base path: {}", targetStage, basePath.string());
    return std::nullopt;
}

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

        const auto& initialPath = metadata.m_filePath;
        if (!std::filesystem::exists(initialPath)) {
            GE_CORE_ERROR("AssetImporter::loadShader - Initial shader file not found: {}", initialPath.string());
            return nullptr;
        }

        // Determine the type of the initial shader file
        std::string initialPathStr = initialPath.string();
        std::regex stageRegex("\\.(vert|vs|frag|fs|geom|gs|comp|cs)\\.[^.]+$");
        std::smatch stageMatch;
        std::string initialStageType;

        if (std::regex_search(initialPathStr, stageMatch, stageRegex) && stageMatch.size() > 1) {
            std::string stageExt = stageMatch[1].str();
            if (stageExt == "vert" || stageExt == "vs") initialStageType = "vertex";
            else if (stageExt == "frag" || stageExt == "fs") initialStageType = "fragment";
            else if (stageExt == "geom" || stageExt == "gs") initialStageType = "geometry";
            else if (stageExt == "comp" || stageExt == "cs") initialStageType = "compute";
        }

        if (initialStageType.empty()) {
            GE_CORE_ERROR("AssetImporter::loadShader - Could not determine shader stage from file name: {}", initialPath.string());
            return nullptr;
        }

        GE_CORE_TRACE("AssetImporter::loadShader - Initial shader type detected as: {}", initialStageType);

        std::shared_ptr<Shader> shader;

        // Handle Compute Shaders (Standalone)
        if (initialStageType == "compute") {
            shader = Shader::createCompute(initialPath);
            if (!shader || !shader->isValid()) {
                GE_CORE_ERROR("AssetImporter::loadShader - Failed to create or compile compute shader from {}, with status {}",
                    initialPath.string(), (int)shader->getStatus());
                return nullptr;
            }
        }
        // Handle Graphics Shaders (Vertex + Fragment required, Geometry optional)
        else {
            // Find required vertex and fragment shaders
            auto vertexPathOpt = getRelatedShaderPath(initialPath, "vertex");
            auto fragmentPathOpt = getRelatedShaderPath(initialPath, "fragment");

            if (!vertexPathOpt) {
                GE_CORE_ERROR("AssetImporter::loadShader - Could not find vertex shader related to: {}", initialPath.string());
                return nullptr;
            }
            if (!fragmentPathOpt) {
                GE_CORE_ERROR("AssetImporter::loadShader - Could not find fragment shader related to: {}", initialPath.string());
                return nullptr;
            }

            // Optionally find geometry shader
            auto geometryPathOpt = getRelatedShaderPath(initialPath, "geometry");

            std::filesystem::path vertexPath = *vertexPathOpt;
            std::filesystem::path fragmentPath = *fragmentPathOpt;

            if (geometryPathOpt) {
                std::filesystem::path geometryPath = *geometryPathOpt;
                shader = Shader::create(vertexPath, fragmentPath, geometryPath);
            } else {
                shader = Shader::create(vertexPath, fragmentPath);
            }

            if (!shader || !shader->isValid()) {
                GE_CORE_ERROR("AssetImporter::loadShader - Failed to create or compile shader from {} and {}{}",
                    vertexPath.string(),
                    fragmentPath.string(),
                    geometryPathOpt ? " and " + geometryPathOpt->string() : ""); 
                return nullptr;
            }
        }


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
