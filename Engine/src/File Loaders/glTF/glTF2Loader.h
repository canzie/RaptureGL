#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <glm/glm.hpp>
#include "json.hpp"

#include "../../DataTypes.h"
#include "../../Buffers/VertexArray.h"
#include "../../Scenes/Scene.h"
#include "../../Scenes/Entity.h"
#include "../../Materials/Material.h"
#include "../../Animations/Animation.h"

using json = nlohmann::json;

namespace Rapture
{
    // Forward declaration
    //class ModelAssetCache;

    /**
     * @brief Structure to hold glTF file metadata
     */
    struct glTFMetadata {
        size_t materialCount = 0;
        size_t primitiveCount = 0;
        size_t animationCount = 0;
        size_t nodeCount = 0;
        size_t meshCount = 0;
        size_t textureCount = 0;
        bool hasSkeletons = false;
        std::string version;
        std::string generator;
    };

    enum class NodeType
    {
        Empty,
        Mesh,
        Bone,
        Skeleton,
    };


	/**
	 * @brief Modern loader for glTF 2.0 format 3D models using entity-component architecture
	 * 
	 * This class handles loading glTF model files and creating entities with appropriate components.
	 * Each glTF node becomes an entity with transform and mesh components as needed.
	 */
	class glTF2Loader
	{
	public:
		/**
		 * @brief Constructor that takes a scene to populate
		 * 
		 * @param scene Pointer to the scene where entities will be created
		 */
		glTF2Loader(std::shared_ptr<Scene> scene);
		
		/**
		 * @brief Destructor
		 */
		~glTF2Loader();

        /**
         * @brief Static method to quickly analyze a glTF file and return metadata
         * 
         * @param filepath Path to the .gltf file
         * @param isAbsolute If true, filepath is an absolute path
         * @return glTFMetadata structure containing file information
         */
        static glTFMetadata getFileMetadata(const std::string& filepath, bool isAbsolute = false);

        bool initialize(const std::string& filepath);
		
		/**
		 * @brief Load a model from a glTF file and populate the scene with entities
		 * 
		 * @param filepath Path to the .gltf file
		 * @param calculateBoundingBoxes If true, bounding boxes will be calculated for all primitives
		 * @return true if loading was successful, false otherwise
		 */
		bool loadModel(const std::string& filepath, bool isAbsolute=false, bool calculateBoundingBoxes = false);

        /**
         * @brief Load animations from a glTF file
         * 
         * @param filepath Path to the .gltf file
         * @param isAbsolute If true, filepath is an absolute path
         * @return std::vector of loaded animations
         */
        std::vector<std::shared_ptr<Animation>> loadAnimations(const std::string& filepath, bool isAbsolute=false);

        /**
         * @brief Load a specific material by index
         * 
         * @param materialIndex Index of the material to load
         * @return std::shared_ptr<Material> The loaded material
         */
        std::shared_ptr<Material> loadMaterialByIndex(size_t materialIndex);



        /**
         * @brief Check if file was successfully loaded
         * 
         * @return true if file is loaded and ready
         */
        bool isLoaded() const { return m_isLoaded; }

        // Friend declaration for the cache
        friend class ModelAssetCache;

	private:
		/**
		 * @brief Process a glTF primitive and set up mesh data
		 * 
		 * @param entity Entity to attach mesh data to
		 * @param primitive JSON object containing primitive data
		 */
		void processPrimitive(Entity entity, json& primitive);

		/**
		 * @brief Extract raw binary data from an accessor
		 * 
		 * @param accessorJSON JSON object containing accessor information
		 * @param data_vec Vector to store the extracted binary data
		 */
		void loadAccessor(json& accessorJSON, std::vector<unsigned char>& data_vec);

		/**
		 * @brief Process a mesh from the glTF file and create entities
		 * 
		 * @param parentEntity Parent entity for this mesh
		 * @param meshJSON JSON object containing mesh data
		 * @return Entity The created entity
		 */
		Entity processMesh(Entity parentEntity, json& meshJSON);

		/**
		 * @brief Process the node hierarchy and create entities with proper transforms
		 * 
		 * @param parentEntity Parent entity
		 * @param nodeJSON JSON object containing node data
		 * @return Entity The created entity
		 */
		NodeType processNode(Entity parentEntity, json& nodeJSON);

        /**
         * @brief Process a skeleton from the glTF file and create entities
         * 
         * @param parentEntity Parent entity
         * @param skinJSON JSON object containing skeleton data
         */
        void processSkeleton(Entity parentEntity, json& skinJSON);

		/**
		 * @brief Process a scene from the glTF file
		 * 
		 * @param sceneJSON JSON object containing scene data
		 * @return Entity The root scene entity
		 */
		void processScene(json& sceneJSON);

        /**
         * @brief Process animations from the glTF file
         * 
         * @param animationsJSON JSON array containing animation data
         * @return Vector of processed animations
         */
        std::vector<std::shared_ptr<Animation>> processAnimations(json& animationsJSON);

        /**
         * @brief Process a single animation from the glTF file
         * 
         * @param animationJSON JSON object containing animation data
         * @return Processed animation
         */
        std::shared_ptr<Animation> processAnimation(json& animationJSON);

        /**
         * @brief Process animation samplers and create animation channels
         * 
         * @param animation Animation to add channels to
         * @param channelsJSON JSON array containing channel data
         * @param samplersJSON JSON array containing sampler data
         */
        void processAnimationChannelsAndSamplers(
            std::shared_ptr<Animation> animation, 
            json& channelsJSON, 
            json& samplersJSON);

		/**
		 * @brief Load a texture from the glTF file and set it on a material
		 *
		 * @param material The material to set the texture on
		 * @param textureName The name of the texture parameter in the material
		 * @param textureIndex The index of the texture in the glTF file
		 * @return true if the texture was loaded and set successfully
		 */
		//bool loadAndSetTexture(std::shared_ptr<Material> material, const std::string& textureName, int textureIndex);
		bool loadAndSetTexture(std::shared_ptr<Material> material, ParameterID paramID, int textureIndex);

        /**
         * @brief Process a PBR material and create a Material from the JSON data
         * 
         * @param materialJSON The JSON object containing the material data
         * @return The created PBR Material
         */
        std::shared_ptr<Material> processPBRMaterial(json& materialJSON);

        /**
         * @brief Process a KHR_materials_pbrSpecularGlossiness extension and create a SpecularGlossinessMaterial
         * 
         * @param materialJSON The JSON object containing the material data with extensions
         * @return The created SpecularGlossinessMaterial
         */
        std::shared_ptr<Material> processSpecularGlossinessMaterial(json& materialJSON);

		/**
		 * @brief Clean up all data after loading
		 */
		void cleanUp();

        /**
         * @brief Report progress of the loading process
         * 
         * @param progress The progress value between 0.0 and 1.0
         */
        void reportProgress(float progress);

        /**
         * @brief Get the transform matrix of a node
         * 
         * @param nodeJSON JSON object containing node data
         * @return The transform matrix of the node
         */
        glm::mat4 getNodeTransform(json& nodeJSON);

        /**
         * @brief Process a bone from the glTF file and create an entity
         * 
         * @param entity Entity to attach the bone to
         * @param boneIndex The index of the bone in the glTF file
         */
        void processBone(Entity entity, unsigned int boneIndex);

        /**
         * @brief Get node name from glTF node index
         * 
         * @param nodeIndex Index of the node in glTF file
         * @return Name of the node
         */
        std::string getNodeName(unsigned int nodeIndex);

        /**
         * @brief Convert glTF interpolation string to InterpolationType
         * 
         * @param interpolation glTF interpolation string
         * @return Corresponding InterpolationType
         */
        InterpolationType getInterpolationType(const std::string& interpolation);

	private:
		// Reference to the scene being populated
		std::shared_ptr<Scene> m_scene;
		
		// JSON components from the glTF file
		json m_glTFfile;
		json m_accessors;
		json m_meshes;
		json m_bufferViews;
		json m_buffers;
		json m_nodes;
		json m_materials;
		json m_animations;
		json m_skins;
		json m_textures;
		json m_images;
		json m_samplers;

		bool m_calculateBoundingBoxes = false;


		// Raw binary data from the .bin file
		std::vector<unsigned char> m_binVec;
		
		// Base path for loading external resources
		std::string m_basePath;
		
        

		// Constants for glTF component types
		static const unsigned int GLTF_FLOAT = 5126;
		static const unsigned int GLTF_UINT = 5125;
		static const unsigned int GLTF_USHORT = 5123;
		static const unsigned int GLTF_SHORT = 5122;
		static const unsigned int GLTF_UBYTE = 5121;
		static const unsigned int GLTF_BYTE = 5120;

        bool m_isLoaded = false;
        bool m_isInitialized = false;
        std::string m_filepath;
	};

    /**
     * @brief Cache for loaded model assets to avoid redundant file operations
     * @note This caching system only works because a reference to the loader will be used in the loader itself...
     *       The loader will call importasset, the import asset will then get the loader which called the importasset...
     *       This way the loader will not get expired atleast until the original loader shared pointer goes out of scope.
     *       very goofy, so watch out for weird bugs. 
     *       (asssumtion) one cenario which will negate the benefits is if the original caller of the loader does not get its loader trough the modelassetcache
     *       and it does not keep a shared pointer to the loader (return value of getLoader).
     */
    class ModelLoadersCache {
    public:
        /**
         * @brief Get a loader for a specific model file
         * 
         * @param filepath Path to the model file
         * @param isAbsolute If true, filepath is an absolute path
         * @return std::shared_ptr<glTF2Loader> A loader instance for the file
         */

        static void init(){
            if (s_initialized) return;
            s_loaders.clear();
            s_initialized = true;
        }

        static std::shared_ptr<glTF2Loader> getLoader(const std::string& filepath, std::shared_ptr<Scene> scene=nullptr){
            if (!s_initialized){
                GE_CORE_ERROR("ModelLoadersCache - Not initialized");
                return nullptr;
            }

            if (s_loaders.find(filepath) != s_loaders.end()) {
                if (auto loader = s_loaders[filepath].lock()){
                    return loader;
                } else {
                    GE_CORE_WARN("ModelLoadersCache - Loader for '{}' expired, removing from cache", filepath);
                    s_loaders.erase(filepath);
                }
            }

            auto loader = std::make_shared<glTF2Loader>(scene);

            {

                std::lock_guard<std::mutex> lock(s_mutex);


                if (!loader->initialize(filepath)){
                    GE_CORE_ERROR("ModelLoadersCache::getLoader - Failed to initialize loader for '{}'", filepath);
                    return nullptr;
                }
                s_loaders[filepath] = loader;
            }


            return loader;
        }

        /**
         * @brief Clear cache entries not used recently
         */
        static void cleanup(){
            if (!s_initialized) return;

            std::lock_guard<std::mutex> lock(s_mutex);
            for (auto it = s_loaders.begin(); it != s_loaders.end();){
                if (it->second.expired() ){
                    GE_CORE_INFO("ModelLoadersCache::cleanup - Loader for '{}' expired, removing from cache", it->first);
                    it = s_loaders.erase(it);
                } else {
                    ++it;
                }
            }
        }

        /**
         * @brief Clear all cached loaders
         */
        static void clear(){
            if (!s_initialized) return;

            std::lock_guard<std::mutex> lock(s_mutex);
            s_loaders.clear();
        }

    friend class glTF2Loader;

    private:
        // Maps the filepath to the loader
        static bool s_initialized;
        static std::map<std::string, std::weak_ptr<glTF2Loader>> s_loaders;
        static std::mutex s_mutex;
    };
}
