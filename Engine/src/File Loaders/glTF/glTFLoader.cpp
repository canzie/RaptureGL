

#include "glTFLoader.h"

#include <fstream>
#include <type_traits>

//#include "../../logger/Log.h"


#include <glm/gtc/type_ptr.hpp>


#define DIRNAME "D:/random/proj/Rapture/Rapture/src/Rapture/Loader/glTF/glTF files/"

#define GLTF_FLOAT  5126
#define GLTF_UINT   5125
#define GLTF_USHORT 5123
#define GLTF_SHORT  5122
#define GLTF_UBYTE  5121
#define GLTF_BYTE   5120


namespace Rapture {

	std::vector<unsigned char> glTFLoader::s_binVec = {};
	json glTFLoader::s_glTFfile = {};

	std::vector<bool> glTFLoader::s_loaded_accessors = {};
	unsigned long long glTFLoader::s_indexBuffer_offset = 0;
	unsigned long long glTFLoader::s_vertexBuffer_offset = 0;

	BufferLayout glTFLoader::s_buffer_layout = BufferLayout();
	
	Mesh* glTFLoader::s_mesh = nullptr;


	std::vector<std::vector<std::pair<size_t, size_t>>> glTFLoader::s_prebuffer_layout = {};
	unsigned int glTFLoader::s_ibo_count = 0;

	unsigned int glTFLoader::s_prev_highest_index = 0;

	json glTFLoader::s_accessors = {};
	json glTFLoader::s_meshes = {};
	json glTFLoader::s_bufferViews = {};
	json glTFLoader::s_buffers = {};
	json glTFLoader::s_nodes = {};



	void glTFLoader::loadMesh(std::string filepath, Mesh* mesh)
	{


		// load the gltf file
		std::ifstream gltf_file(DIRNAME + filepath); // read .glTF file
		if (!gltf_file)
		{
			GE_CORE_WARN("Couldn't load glTF file");
			return;
		}

		//json json_file;
		gltf_file >> s_glTFfile;
		gltf_file.close();

		// load the bin file with all the buffer data
		std::string bufferURI = s_glTFfile["buffers"][0]["uri"];
		std::ifstream binary_file(DIRNAME + bufferURI, std::ios::binary); // read .bin file
		if (!binary_file)
		{
			GE_CORE_ERROR("Couldn't load binary file");
			return;
		}
		binary_file.unsetf(std::ios::skipws); // Stop eating new lines in binary mode

		s_binVec.reserve(s_glTFfile["buffers"][0]["byteLength"]); // reserve bytes

		s_binVec.insert(s_binVec.begin(), // copy the bin data into a vector
			std::istream_iterator<char>(binary_file),
			std::istream_iterator<char>());
		
		binary_file.close();

		// setup shit here
		s_accessors = s_glTFfile["accessors"];
		s_meshes = s_glTFfile["meshes"];
		s_bufferViews = s_glTFfile["bufferViews"];
		s_buffers = s_glTFfile["buffers"];
		s_nodes = s_glTFfile["nodes"];


		//s_materials = s_glTFfile.value("materials", ...);
		//s_animations = s_glTFfile["animations"];
		//s_skins = s_glTFfile["skins"];

		s_buffer_layout = BufferLayout();


		s_loaded_accessors.resize(s_accessors.size(), false);
		s_indexBuffer_offset = 0;
		s_vertexBuffer_offset = 0;
		s_prev_highest_index = 0;

		s_mesh = mesh;

		//s_VAO.reset(new VertexArray());

		// assumes all indices use the same component type and every primitive has the same vertex layout.
		mesh->getVAO()->setVertexBuffer(getVertexBufferLength());
		
		if (s_meshes[0]["primitives"][0].contains("indices"))
		{
			unsigned int indidx = s_meshes[0]["primitives"][0]["indices"];
			unsigned int cType = s_accessors[indidx]["componentType"];
			mesh->getVAO()->setIndexBuffer(getIndexBufferLength(), cType);
		}



		GE_CORE_TRACE("IndexBuffer Size: {}", getIndexBufferLength());
		GE_CORE_TRACE("VertexBuffer Size: {}", getVertexBufferLength());

		
		// load the vertex attributes
		json attribs = s_meshes[0]["primitives"][0]["attributes"];
		s_prebuffer_layout.resize(attribs.size()); 
		for (auto& attrib : attribs.items())
		{
			std::string name = attrib.key();
			if (name == "COLOR_0") continue;
			json& accessor = s_accessors[(unsigned int)attrib.value()];
			
			unsigned int componentType = accessor["componentType"];
			std::string type = accessor["type"];
			s_buffer_layout.buffer_attribs.push_back({name, componentType, type, 0});
		}

		//s_buffer_layout.buffer_attribs.push_back({"TRANSFORM_MAT", GLTF_FLOAT, "MAT4", 0});
		

		// load all submeshes
		for (unsigned int meshIdx = 0; meshIdx < s_meshes.size(); meshIdx++)
		{	
			//s_submeshes.push_back(nullptr);
			//s_submeshes.back().reset(new SubMesh());
			std::shared_ptr<SubMesh> smesh = mesh->addSubMesh();

			loadMeshData(s_meshes[meshIdx], smesh);

		}
		s_buffer_layout.print_buffer_layout();

		if (s_glTFfile.contains("nodes"))
			loadTransforms();


		mesh->getVAO()->setBufferLayout(s_buffer_layout);
		mesh->getVAO()->getVertexBuffer()->pushData2Buffer(s_prebuffer_layout);
		
		//submeshes = s_submeshes;

		cleanFiles();
		
	}

	// appends the correct data to the input vectors(vertex en index buffers)
	void glTFLoader::loadMeshData(json& meshJSON, std::shared_ptr<SubMesh> submesh)
	{
		GE_CORE_INFO("loading Primitive: {0}", meshJSON.value("name", "Untitled"));

		submesh->m_name = meshJSON.value("name", "No Name");
		

		auto& sub_primitives = meshJSON["primitives"];
		for (size_t i = 0; i < sub_primitives.size(); i++)
		{
			loadPrimitive(sub_primitives[i], submesh);
		}

		// scuffed temp fix for
		// only update the s_indexBuffer_offset after getting all the indices that reffer to the same vertces
		//unsigned int posIdx = sub_primitives[0]["attributes"]["POSITION"];
		//int highest_index = s_accessors[posIdx]["count"];

		//s_indexBuffer_offset += highest_index;

	}



	unsigned long long glTFLoader::getIndexBufferLength()
	{
		unsigned long long total_bytes = 0;

		if (!s_meshes[0]["primitives"][0].contains("indices")) return 0;

		// loop for every mesh
		for (size_t i = 0; i < s_meshes.size(); i++)
		{
			json& primitives = s_meshes[i]["primitives"];
			// loop for every primitive
			for (size_t j = 0; j < primitives.size(); j++)
			{

				unsigned int indicesIdx = primitives[j]["indices"];
				total_bytes += getAccessorByteSize(indicesIdx);
			}
		}
		return total_bytes;
	}

	unsigned long long glTFLoader::getVertexBufferLength()
	{
		unsigned long long total_bytes = 0;
		std::vector<bool> visited_accessors;
		visited_accessors.resize(s_accessors.size(), false);
		
		// loop for every mesh
		for (size_t i = 0; i < s_meshes.size(); i++)
		{
			json& primitives = s_meshes[i]["primitives"];
			// loop for every primitive
			for (size_t j = 0; j < primitives.size(); j++)
			{

				json& attribs = primitives[j]["attributes"];
				// loop for every attribute
				for (auto& attrib : attribs.items())
				{
					if (!visited_accessors[(unsigned int)attrib.value()] && attrib.key()!="COLOR_0")
					{
						visited_accessors[(unsigned int)attrib.value()] = true;
						total_bytes += getAccessorByteSize((unsigned int)attrib.value());
					}
				}

				// size for transformation matrix (found in s_nodes)
				// total_bytes += sizeof(glm::mat4);

			}
		}
		return total_bytes;
	}

	unsigned int glTFLoader::getAccessorByteSize(unsigned int accessor_idx)
	{
		unsigned int byte_size = 0;
		json& accessor = s_accessors[accessor_idx];
		unsigned int count = accessor["count"];
		unsigned int cType = accessor["componentType"];
		std::string type = accessor["type"];

		byte_size = count;

		switch (cType)
		{
		case GLTF_BYTE: case GLTF_UBYTE: byte_size *= 1; break;
		case GLTF_USHORT: case GLTF_SHORT: byte_size *= 2; break;
		case GLTF_FLOAT: case GLTF_UINT: byte_size *= 4; break;
		}

		//if (type == "SCALAR") byte_size *= 1;
		if (type == "VEC2") byte_size *= 2;
		else if (type == "VEC3") byte_size *= 3;
		else if (type == "VEC4") byte_size *= 4;

		return byte_size;
	}


	void glTFLoader::cleanFiles()
	{
		s_glTFfile.clear();
		s_binVec = std::vector<unsigned char>();
		s_loaded_accessors = std::vector<bool>();
		s_prebuffer_layout.clear();
		s_ibo_count = 0;
	}



	void glTFLoader::loadPrimitive(json& primitive, std::shared_ptr<SubMesh> submesh)
	{

		int indicesInd = primitive.value("indices", -1);

		std::vector<unsigned char> data_vec;

		unsigned int posIdx = primitive["attributes"]["POSITION"];


		if (indicesInd != -1)
		{

			unsigned int indCount = s_accessors[indicesInd]["count"];


			if (!s_loaded_accessors[posIdx])
			{
				s_indexBuffer_offset += s_prev_highest_index;
				s_prev_highest_index = s_accessors[posIdx]["count"];
			}

			if (submesh->getIndexCount() == 0) // fresh submesh
			{
				submesh->setPartition(indCount, s_ibo_count);
			}
			else
			{
				submesh->setPartition(indCount, 0);
			}


			s_ibo_count += indCount * 2;

			loadAccessor(s_accessors[indicesInd], data_vec);

			unsigned int comp_type = s_accessors[indicesInd]["componentType"];

			if (comp_type == GLTF_SHORT)
				add2BinaryVector<short>(data_vec, 2);
			else if (comp_type == GLTF_USHORT)
				add2BinaryVector<unsigned short>(data_vec, 2);
			else if (comp_type == GLTF_UINT)
				add2BinaryVector<unsigned int>(data_vec, 4);

			unsigned short tst = reinterpret_cast<unsigned short*>(&data_vec[0])[0];

			submesh->getParentMesh()->getVAO()->getIndexBuffer()->addSubIndices(data_vec);
		}

		size_t offset = 0;
		int i = 0; //attrib index

		for (auto& attrib : primitive["attributes"].items())
		{
			if (s_loaded_accessors[(unsigned int)attrib.value()] || std::string(attrib.key()) == "COLOR_0") continue;

			loadAccessor(s_accessors[(unsigned int)attrib.value()], data_vec);
			submesh->getParentMesh()->getVAO()->getVertexBuffer()->addSubData(data_vec);
			

			s_buffer_layout.getAttribute(attrib.key()).offset += offset;

			s_prebuffer_layout[i].push_back({ s_vertexBuffer_offset, s_vertexBuffer_offset + data_vec.size() });

			s_vertexBuffer_offset += data_vec.size();
			offset += data_vec.size();
			
			s_loaded_accessors[(unsigned int)attrib.value()] = true;
			i++;
		}

		/*
		// float->binary vector
		data_vec.resize(64);
		int a = 0;
		//glm::mat4 transform_mat = s_submesh_transforms[s_submeshes.size()-1];
		glm::mat4 transform_mat(1.0f);

		for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++)
		{
			float tmp = transform_mat[y][x];
			unsigned char const* p = reinterpret_cast<unsigned char const*>(&tmp);
			data_vec[a++] = p[0];
			data_vec[a++] = p[1];
			data_vec[a++] = p[2];
			data_vec[a++] = p[3];
		}
		s_VAO->getVertexBuffer()->addSubData(data_vec);

		// dont forget the transform matrix
		s_buffer_layout.getAttribute("TRANSFORM_MAT").offset += offset;
		s_prebuffer_layout[i].push_back({ s_vertexBuffer_offset, s_vertexBuffer_offset + sizeof(glm::mat4) });

		s_vertexBuffer_offset += sizeof(glm::mat4);
		*/

	}

	void glTFLoader::loadAccessor(json& accessorJSON, std::vector<unsigned char>& dataVec)
	{
		
		unsigned int bufferviewInd = accessorJSON.value("bufferView", 0);
		unsigned int count = accessorJSON["count"];
		unsigned int componentType = accessorJSON["componentType"]; // float, byte, ...

		size_t accbyteOffset = accessorJSON.value("byteOffset", 0); // offset in bufferview, 0 if not preset
		std::string type = accessorJSON["type"];

		json bufferView = s_bufferViews[bufferviewInd];
		size_t byteOffset = bufferView.value("byteOffset", 0) + accbyteOffset;
		unsigned int byteStride = 1; // default, if type is "SCALAR"

		if (type == "VEC2") byteStride = 2;
		else if (type == "VEC3") byteStride = 3;
		else if (type == "VEC4") byteStride = 4;



		switch (componentType)
		{
		case GLTF_BYTE: case GLTF_UBYTE: byteStride *= 1; break;
		case GLTF_USHORT: case GLTF_SHORT: byteStride *= 2; break;
		case GLTF_FLOAT: case GLTF_UINT: byteStride *= 4; break;
		}

		byteStride *= count;

		dataVec = std::vector(s_binVec.begin() + byteOffset, s_binVec.begin() + byteOffset + byteStride);

		
	}

	
	template <typename T>
	void glTFLoader::add2BinaryVector(std::vector<unsigned char>& bin_vec, unsigned int bytrs_per_component)
	{
		for (size_t i = 0; i < bin_vec.size(); i+= bytrs_per_component)
		{
		
			auto test = reinterpret_cast<T*>(&bin_vec[i]);
			*test += s_indexBuffer_offset;

		}
		
	}

	void glTFLoader::loadTransforms()
	{
		glm::mat4 root_mat(1.0f);
		unsigned int root_node = s_glTFfile["scenes"][0]["nodes"][0];
		traverseNode(root_mat, s_nodes[root_node]);
	}

	void glTFLoader::traverseNode(glm::mat4& transformMatrix, json& node)
	{
		glm::vec3 translationV(0.0f);
		glm::vec3 scaleV(1.0f);
		glm::quat rotationQ(1.0f, 0.0f, 0.0f, 0.0f);

		//GE_CORE_TRACE(node["name"]);
		if (node.contains("translation"))
			translationV = { node["translation"][0], node["translation"][1],node["translation"][2] };
		if (node.contains("rotation")) // wxyz(gltf) -> xyzw(opengl)
			rotationQ = { node["rotation"][3], node["rotation"][0], node["rotation"][1],node["rotation"][2] };
		if (node.contains("scale")) // wxyz(gltf) -> xyzw(opengl)
			scaleV = { node["scale"][0], node["scale"][1],node["scale"][2] };

		// apply the shit to the matrix
		glm::mat4 nodeMatrix = glm::mat4(1.0f);
		
		if (node.contains("matrix"))
		{
			float node_values[16];
			for (int i = 0; i < 16; i++)
			{
				node_values[i] = node["matrix"][i];
			}
			nodeMatrix = glm::make_mat4(node_values);
		}

		nodeMatrix = transformMatrix * nodeMatrix * glm::translate(translationV) * glm::mat4_cast(rotationQ) * glm::scale(scaleV);


		// apply the matrix to the mesh (if present)
		if (node.contains("mesh"))
		{
			s_mesh->getSubMeshes()[node["mesh"]]->setTransform(nodeMatrix);
		}

		// traverse further if the children are present
		if (node.contains("children"))
		{
			unsigned int childIDX;
			for (int i = 0; i < node["children"].size(); i++)
			{
				childIDX = node["children"][i];
				traverseNode(nodeMatrix, s_glTFfile["nodes"][childIDX]);
			}
		}
	}

}

