#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <glm/glm.hpp>
#include "json.hpp"

#include "../../DataTypes.h"
#include "../../Buffers/VertexArray.h"
#include "../../Scenes/Scene.h"
#include "../../Scenes/Entity.h"
#include "../../Materials/Material.h"

using json = nlohmann::json;

namespace Rapture
{

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
		 * @brief Load a model from a glTF file and populate the scene with entities
		 * 
		 * @param filepath Path to the .gltf file
		 * @param calculateBoundingBoxes If true, bounding boxes will be calculated for all primitives
		 * @return true if loading was successful, false otherwise
		 */
		bool loadModel(const std::string& filepath, bool isAbsolute=false, bool calculateBoundingBoxes = false);

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
		Entity processNode(Entity parentEntity, json& nodeJSON);

		/**
		 * @brief Process a scene from the glTF file
		 * 
		 * @param sceneJSON JSON object containing scene data
		 * @return Entity The root scene entity
		 */
		void processScene(json& sceneJSON);

		/**
		 * @brief Load a texture from the glTF file and set it on a material
		 *
		 * @param material The material to set the texture on
		 * @param textureName The name of the texture parameter in the material
		 * @param textureIndex The index of the texture in the glTF file
		 * @return true if the texture was loaded and set successfully
		 */
		bool loadAndSetTexture(std::shared_ptr<Material> material, const std::string& textureName, int textureIndex);

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
	};
}
