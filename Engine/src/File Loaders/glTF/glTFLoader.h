#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include "json.hpp"

#include "../../DataTypes.h"

#include "../../Mesh/SubMesh.h"

//#include "../../Buffers/VertexArray.h"
//#include <memory>



using json = nlohmann::json;


namespace Rapture
{
	class glTFLoader
	{
	public:
		glTFLoader() = default;
		//glTFLoader(std::string filepath, std::vector<vertex>& vertices, std::vector<unsigned int>& indices, unsigned int meshInd);
		//static void loadMesh(std::string filepath, std::vector<vertex>& vertices, std::vector<unsigned int>& indices, unsigned int meshInd);
		static void loadMesh(std::string filepath, Mesh* meshes);

	private:

		static void loadPrimitive(json& primitive, std::shared_ptr<SubMesh> submesh);

		static void loadAccessor(json& accessorJSON, std::vector<unsigned char>& data_vec);

		static void loadMeshData(json& mesh, std::shared_ptr<SubMesh> submesh);

		static void loadTransforms();

		static unsigned long long getIndexBufferLength();
		static unsigned long long getVertexBufferLength();

		template <typename T>
		static void add2BinaryVector(std::vector<unsigned char>& bin_vec, unsigned int bytrs_per_component);

		static unsigned int getAccessorByteSize(unsigned int accessor_idx);

		static void traverseNode(glm::mat4& transformMatrix, json& node);

		static void cleanFiles();

	private:
		static json s_glTFfile;

		static json s_accessors;
		static json s_meshes;
		static json s_bufferViews;
		static json s_buffers;
		static json s_nodes;
		static json s_materials;
		static json s_animations;
		static json s_skins;

		static Mesh* s_mesh;

		static unsigned int s_ibo_count;

		static unsigned int s_prev_highest_index;

		static std::vector<bool> s_loaded_accessors;
		static unsigned long long s_indexBuffer_offset;
		static unsigned long long s_vertexBuffer_offset;

		static BufferLayout s_buffer_layout;
		// description of how the data in the given buffer should be re-ordered/re-positioned;
		static std::vector<std::vector<std::pair<size_t, size_t>>> s_prebuffer_layout;

		static std::vector<unsigned char> s_binVec;
	};

}