
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include "json.hpp"

#include "../../DataTypes.h"

#include "../../Mesh/SubMesh.h"
#include "../../Scenes/Entity.h"

//#include "../../Buffers/VertexArray.h"
//#include <memory>

using json = nlohmann::json;

namespace Rapture
{
	/**
	 * @brief Loader for glTF format 3D models
	 * 
	 * This class handles loading glTF model files and preparing them for rendering.
	 * It processes both the JSON structure data and the binary buffer data,
	 * organized vertices, indices, and other attributes into efficient memory layouts.
	 */
	class glTFLoader
	{
	public:
		glTFLoader() = default;
		
		/**
		 * @brief Load a mesh from a glTF file
		 * 
		 * @param filepath Path to the .gltf file
		 * @param mesh Pointer to the mesh where data should be loaded
		 */
		static void loadMesh(std::string filepath, Mesh* mesh);

	private:
		/**
		 * @brief Process a single primitive from the glTF file
		 * 
		 * @param primitive JSON object containing primitive data
		 * @param submesh SubMesh to store the primitive data
		 */
		static void loadPrimitive(json& primitive, std::shared_ptr<SubMesh> submesh);

		/**
		 * @brief Extract raw binary data from an accessor
		 * 
		 * @param accessorJSON JSON object containing accessor information
		 * @param data_vec Vector to store the extracted binary data
		 */
		static void loadAccessor(json& accessorJSON, std::vector<unsigned char>& data_vec);

		/**
		 * @brief Process a mesh from the glTF file
		 * 
		 * @param mesh JSON object containing mesh data
		 * @param submesh SubMesh to store the mesh data
		 */
		static void loadMeshData(json& mesh, std::shared_ptr<SubMesh> submesh);

		/**
		 * @brief Load transformation data from the node hierarchy
		 */
		static void loadTransforms();

		/**
		 * @brief Calculate the total size needed for index buffer
		 * 
		 * @return Total size in bytes
		 */
		static unsigned long long getIndexBufferLength();
		
		/**
		 * @brief Calculate the total size needed for vertex buffer
		 * 
		 * @return Total size in bytes
		 */
		static unsigned long long getVertexBufferLength();

		/**
		 * @brief Adjust index values to account for buffer offsets
		 * 
		 * @param bin_vec Binary vector containing index data
		 * @param bytes_per_component Size of each component in bytes
		 */
		template <typename T>
		static void add2BinaryVector(std::vector<unsigned char>& bin_vec, unsigned int bytes_per_component);

		/**
		 * @brief Calculate the size of an accessor's data in bytes
		 * 
		 * @param accessor_idx Index of the accessor in the accessors array
		 * @return Size in bytes
		 */
		static unsigned int getAccessorByteSize(unsigned int accessor_idx);

		/**
		 * @brief Process a node in the glTF scene hierarchy
		 * 
		 * @param transformMatrix Parent transform matrix
		 * @param node JSON object containing node data
		 */
		static void traverseNode(glm::mat4& transformMatrix, json& node);

		/**
		 * @brief Clean up all static data after loading
		 */
		static void cleanFiles();

	private:
		// JSON components from the glTF file
		static json s_glTFfile;
		static json s_accessors;
		static json s_meshes;
		static json s_bufferViews;
		static json s_buffers;
		static json s_nodes;
		static json s_materials;
		static json s_animations;
		static json s_skins;

		// Reference to the mesh being loaded
		static Mesh* s_mesh;

		// Index buffer tracking
		static unsigned int s_ibo_count;
		static unsigned int s_prev_highest_index;

		// Tracking for accessor loading
		static std::vector<bool> s_loaded_accessors;
		static unsigned long long s_indexBuffer_offset;
		static unsigned long long s_vertexBuffer_offset;

		// Buffer layout and organization
		static BufferLayout s_buffer_layout;
		
		// Description of how the data in the given buffer should be re-ordered/re-positioned
		static std::vector<std::vector<std::pair<size_t, size_t>>> s_prebuffer_layout;

		// Raw binary data from the .bin file
		static std::vector<unsigned char> s_binVec;
	};
}