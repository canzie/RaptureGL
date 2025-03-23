/*


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
	json glTFLoader::s_materials = {};
	json glTFLoader::s_animations = {};
	json glTFLoader::s_skins = {};



	void glTFLoader::loadMesh(std::string filepath, Mesh* mesh)
	{
		GE_CORE_INFO("GLTFLoader: Loading mesh from {}", filepath);
		// Reset static state to ensure clean loading
		cleanFiles();
		
		s_mesh = mesh;

		// Load the gltf file
		std::ifstream gltf_file(DIRNAME + filepath);
		if (!gltf_file)
		{
			GE_CORE_ERROR("GLTFLoader: Couldn't load glTF file '{}'", filepath);
			return;
		}

		// Parse the JSON file
		try {
			gltf_file >> s_glTFfile;
		}
		catch (const std::exception& e) {
			GE_CORE_ERROR("GLTFLoader: Failed to parse glTF JSON: {}", e.what());
			return;
		}
		gltf_file.close();

		// Load references to major sections
		s_accessors = s_glTFfile.value("accessors", json::array());
		s_meshes = s_glTFfile.value("meshes", json::array());
		s_bufferViews = s_glTFfile.value("bufferViews", json::array());
		s_buffers = s_glTFfile.value("buffers", json::array());
		s_nodes = s_glTFfile.value("nodes", json::array());
		s_materials = s_glTFfile.value("materials", json::array());
		s_animations = s_glTFfile.value("animations", json::array());
		s_skins = s_glTFfile.value("skins", json::array());

		// Validate required sections
		if (s_accessors.empty() || s_meshes.empty() || s_bufferViews.empty() || s_buffers.empty()) {
			GE_CORE_ERROR("GLTFLoader: Missing required glTF sections");
			return;
		}

		// Load the bin file with all the buffer data
		std::string bufferURI = s_buffers[0].value("uri", "");
		if (bufferURI.empty()) {
			GE_CORE_ERROR("GLTFLoader: Buffer URI is missing");
			return;
		}
		
		std::ifstream binary_file(DIRNAME + bufferURI, std::ios::binary);
		if (!binary_file)
		{
			GE_CORE_ERROR("GLTFLoader: Couldn't load binary file '{}'", bufferURI);
			return;
		}
		
		// Get file size and reserve space
		binary_file.seekg(0, std::ios::end);
		size_t fileSize = binary_file.tellg();
		binary_file.seekg(0, std::ios::beg);
		
		s_binVec.resize(fileSize);
		
		// Read the entire file at once for efficiency
		if (!binary_file.read(reinterpret_cast<char*>(s_binVec.data()), fileSize)) {
			GE_CORE_ERROR("GLTFLoader: Failed to read binary data");
			return;
		}
		
		binary_file.close();

		// Setup for buffer loading
		s_buffer_layout = BufferLayout();
		s_loaded_accessors.resize(s_accessors.size(), false);
		s_indexBuffer_offset = 0;
		s_vertexBuffer_offset = 0;
		s_prev_highest_index = 0;

		// Calculate buffer sizes
		unsigned long long vertexBufferSize = getVertexBufferLength();
		unsigned long long indexBufferSize = getIndexBufferLength();
		
		// Setup VAO and buffers
		if (vertexBufferSize == 0) {
			GE_CORE_ERROR("GLTFLoader: No vertex data found");
			return;
		}
		
		mesh->getVAO()->setVertexBuffer(vertexBufferSize);
		
		// Only create index buffer if indices exist
		if (indexBufferSize > 0) {
			unsigned int indexComponentType = GLTF_USHORT; // Default
			
			// Find first primitive with indices to determine component type
			if (s_meshes[0]["primitives"][0].contains("indices")) {
				unsigned int indidx = s_meshes[0]["primitives"][0]["indices"];
				indexComponentType = s_accessors[indidx]["componentType"];
			}
			
			mesh->getVAO()->setIndexBuffer(indexBufferSize, indexComponentType);
		}

		GE_CORE_INFO("GLTFLoader: Buffer sizes - Vertex: {} bytes, Index: {} bytes", 
			vertexBufferSize, indexBufferSize);

		// Analyze the attribute structure from the first primitive
		if (!s_meshes[0]["primitives"].empty() && s_meshes[0]["primitives"][0].contains("attributes")) {
			json attribs = s_meshes[0]["primitives"][0]["attributes"];
			s_prebuffer_layout.resize(attribs.size());
			
			// Process each attribute type (position, normal, etc.)
			for (auto& attrib : attribs.items())
			{
				std::string name = attrib.key();
				if (name == "COLOR_0") continue; // Skip color data
				
				json& accessor = s_accessors[(unsigned int)attrib.value()];
				
				unsigned int componentType = accessor["componentType"];
				std::string type = accessor["type"];
				s_buffer_layout.buffer_attribs.push_back({name, componentType, type, 0});
			}
		}

		// Load all submeshes
		for (unsigned int meshIdx = 0; meshIdx < s_meshes.size(); meshIdx++)
		{	
			std::shared_ptr<SubMesh> smesh = mesh->addSubMesh();
			
			// Process this mesh
			loadMeshData(s_meshes[meshIdx], smesh);
		}
		
		// Print layout info
		s_buffer_layout.print_buffer_layout();

		// Load transformation matrices from nodes if present
		if (s_glTFfile.contains("nodes")) {
			loadTransforms();
		}

		// Finalize buffer setup
		mesh->getVAO()->setBufferLayout(s_buffer_layout);
		
		// Process and push the efficiently organized data to GPU
		if (!s_prebuffer_layout.empty()) {
			mesh->getVAO()->getVertexBuffer()->pushData2Buffer(s_prebuffer_layout);
		}
		
		// Clean up
		cleanFiles();
	}

	// Appends the correct data to the input vectors (vertex and index buffers)
	void glTFLoader::loadMeshData(json& meshJSON, std::shared_ptr<SubMesh> submesh)
	{
		GE_CORE_INFO("GLTFLoader: Loading mesh '{}' with {} primitives", 
			meshJSON.value("name", "Untitled"), 
			meshJSON.value("primitives", json::array()).size());

		submesh->m_name = meshJSON.value("name", "Untitled");
		
		auto& primitives = meshJSON["primitives"];
		if (primitives.empty()) {
			GE_CORE_WARN("GLTFLoader: Mesh '{}' has no primitives", submesh->m_name);
			return;
		}

		for (size_t i = 0; i < primitives.size(); i++) {
			loadPrimitive(primitives[i], submesh);
		}
	}

	void glTFLoader::loadPrimitive(json& primitive, std::shared_ptr<SubMesh> submesh)
	{
		// Process indices if present
		int indicesInd = primitive.value("indices", -1);
		if (indicesInd != -1)
		{
			std::vector<unsigned char> data_vec;
			unsigned int posIdx = primitive["attributes"]["POSITION"];
			unsigned int indCount = s_accessors[indicesInd]["count"];

			// Update index offsets
			if (!s_loaded_accessors[posIdx]) {
				s_indexBuffer_offset += s_prev_highest_index;
				s_prev_highest_index = s_accessors[posIdx]["count"];
			}

			// Set up submesh partition
			if (submesh->getIndexCount() == 0) {
				submesh->setPartition(indCount, s_ibo_count);
			}
			else {
				submesh->setPartition(indCount, 0);
			}

			s_ibo_count += indCount * 2;

			// Load index data
			loadAccessor(s_accessors[indicesInd], data_vec);

			// Adjust indices to account for vertex offsets
			unsigned int comp_type = s_accessors[indicesInd]["componentType"];
			switch (comp_type) {
				case GLTF_SHORT:
					add2BinaryVector<short>(data_vec, 2);
					break;
				case GLTF_USHORT:
					add2BinaryVector<unsigned short>(data_vec, 2);
					break;
				case GLTF_UINT:
					add2BinaryVector<unsigned int>(data_vec, 4);
					break;
				default:
					GE_CORE_WARN("GLTFLoader: Unsupported index component type: {}", comp_type);
			}

			submesh->getParentMesh()->getVAO()->getIndexBuffer()->addSubIndices(data_vec);
		}

		// Process vertex attributes
		size_t offset = 0;
		int i = 0; // attribute index
		for (auto& attrib : primitive["attributes"].items())
		{
			// Skip already loaded attributes and color data
			if (s_loaded_accessors[(unsigned int)attrib.value()] || std::string(attrib.key()) == "COLOR_0") continue;

			// Load attribute data
			std::vector<unsigned char> data_vec;
			loadAccessor(s_accessors[(unsigned int)attrib.value()], data_vec);
			
			if (data_vec.empty()) {
				GE_CORE_WARN("GLTFLoader: Empty data for attribute '{}'", attrib.key());
				continue;
			}
			
			// Add the data to the vertex buffer
			submesh->getParentMesh()->getVAO()->getVertexBuffer()->addSubData(data_vec);
			
			// Update buffer layout information
			s_buffer_layout.getAttribute(attrib.key()).offset += offset;
			
			// Record data placement for later optimization
			if (i < s_prebuffer_layout.size()) {
				s_prebuffer_layout[i].push_back({ 
					s_vertexBuffer_offset, 
					s_vertexBuffer_offset + data_vec.size() 
				});
			}

			s_vertexBuffer_offset += data_vec.size();
			offset += data_vec.size();
			
			s_loaded_accessors[(unsigned int)attrib.value()] = true;
			i++;
		}
	}

	unsigned long long glTFLoader::getIndexBufferLength()
	{
		unsigned long long total_bytes = 0;

		// Check if we have any indices
		if (s_meshes.empty() || 
			s_meshes[0]["primitives"].empty() || 
			!s_meshes[0]["primitives"][0].contains("indices")) {
			return 0;
		}

		// Calculate total index buffer size
		for (size_t i = 0; i < s_meshes.size(); i++)
		{
			json& primitives = s_meshes[i]["primitives"];
			for (size_t j = 0; j < primitives.size(); j++)
			{
				if (!primitives[j].contains("indices")) continue;
				
				unsigned int indicesIdx = primitives[j]["indices"];
				if (indicesIdx >= s_accessors.size()) {
					GE_CORE_WARN("GLTFLoader: Invalid accessor index: {}", indicesIdx);
					continue;
				}
				
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
		
		// Calculate total vertex buffer size
		for (size_t i = 0; i < s_meshes.size(); i++)
		{
			json& primitives = s_meshes[i]["primitives"];
			for (size_t j = 0; j < primitives.size(); j++)
			{
				if (!primitives[j].contains("attributes")) continue;
				
				json& attribs = primitives[j]["attributes"];
				for (auto& attrib : attribs.items())
				{
					unsigned int accessor_idx = (unsigned int)attrib.value();
					if (accessor_idx >= s_accessors.size()) {
						GE_CORE_WARN("GLTFLoader: Invalid accessor index: {}", accessor_idx);
						continue;
					}
					
					if (!visited_accessors[accessor_idx] && attrib.key() != "COLOR_0")
					{
						visited_accessors[accessor_idx] = true;
						total_bytes += getAccessorByteSize(accessor_idx);
					}
				}
			}
		}
		return total_bytes;
	}

	unsigned int glTFLoader::getAccessorByteSize(unsigned int accessor_idx)
	{
		if (accessor_idx >= s_accessors.size()) {
			GE_CORE_ERROR("GLTFLoader: Accessor index out of range: {}", accessor_idx);
			return 0;
		}
		
		json& accessor = s_accessors[accessor_idx];
		unsigned int count = accessor.value("count", 0);
		unsigned int cType = accessor.value("componentType", 0);
		std::string type = accessor.value("type", "SCALAR");

		unsigned int bytes_per_component = 0;
		unsigned int components_per_element = 1;

		// Determine bytes per component based on type
		switch (cType)
		{
		case GLTF_BYTE: case GLTF_UBYTE: bytes_per_component = 1; break;
		case GLTF_USHORT: case GLTF_SHORT: bytes_per_component = 2; break;
		case GLTF_FLOAT: case GLTF_UINT: bytes_per_component = 4; break;
		default:
			GE_CORE_ERROR("GLTFLoader: Unknown component type: {}", cType);
			return 0;
		}

		// Determine number of components per element based on type
		if (type == "SCALAR") components_per_element = 1;
		else if (type == "VEC2") components_per_element = 2;
		else if (type == "VEC3") components_per_element = 3;
		else if (type == "VEC4") components_per_element = 4;
		else if (type == "MAT2") components_per_element = 4;
		else if (type == "MAT3") components_per_element = 9;
		else if (type == "MAT4") components_per_element = 16;
		else {
			GE_CORE_ERROR("GLTFLoader: Unknown data type: {}", type);
			return 0;
		}

		return count * bytes_per_component * components_per_element;
	}

	void glTFLoader::loadAccessor(json& accessorJSON, std::vector<unsigned char>& dataVec)
	{
		// Clear output vector
		dataVec.clear();
		
		// Validate accessor has necessary fields
		if (!accessorJSON.contains("count") || !accessorJSON.contains("componentType") || !accessorJSON.contains("type")) {
			GE_CORE_ERROR("GLTFLoader: Accessor is missing required fields");
			return;
		}
		
		unsigned int bufferviewInd = accessorJSON.value("bufferView", 0);
		if (bufferviewInd >= s_bufferViews.size()) {
			GE_CORE_ERROR("GLTFLoader: Buffer view index out of range: {}", bufferviewInd);
			return;
		}
		
		unsigned int count = accessorJSON["count"];
		unsigned int componentType = accessorJSON["componentType"];
		size_t accbyteOffset = accessorJSON.value("byteOffset", 0);
		std::string type = accessorJSON["type"];

		json& bufferView = s_bufferViews[bufferviewInd];
		size_t byteOffset = bufferView.value("byteOffset", 0) + accbyteOffset;
		unsigned int byteStride = bufferView.value("byteStride", 0);
		
		// Calculate element size
		unsigned int elementSize = 1; // default for SCALAR
		if (type == "VEC2") elementSize = 2;
		else if (type == "VEC3") elementSize = 3;
		else if (type == "VEC4") elementSize = 4;
		
		unsigned int componentSize = 0;
		switch (componentType)
		{
		case GLTF_BYTE: case GLTF_UBYTE: componentSize = 1; break;
		case GLTF_USHORT: case GLTF_SHORT: componentSize = 2; break;
		case GLTF_FLOAT: case GLTF_UINT: componentSize = 4; break;
		}
		
		// Total bytes for this accessor
		unsigned int totalBytes = count * elementSize * componentSize;
		
		// Check if we need to handle interleaved data with stride
		if (byteStride > 0 && byteStride != (elementSize * componentSize)) {
			// Data is interleaved, need to copy with stride
			dataVec.resize(totalBytes);
			
			unsigned int elementBytes = elementSize * componentSize;
			unsigned int dstOffset = 0;
			
			for (unsigned int i = 0; i < count; i++) {
				if (byteOffset + i * byteStride + elementBytes > s_binVec.size()) {
					GE_CORE_ERROR("GLTFLoader: Buffer access out of bounds");
					dataVec.clear();
					return;
				}
				
				std::memcpy(
					dataVec.data() + dstOffset,
					s_binVec.data() + byteOffset + i * byteStride,
					elementBytes
				);
				dstOffset += elementBytes;
			}
		} else {
			// Data is tightly packed, can copy in one go
			if (byteOffset + totalBytes > s_binVec.size()) {
				GE_CORE_ERROR("GLTFLoader: Buffer access out of bounds: offset={}, size={}, buffer size={}", 
					byteOffset, totalBytes, s_binVec.size());
				return;
			}
			
			dataVec.resize(totalBytes);
			std::memcpy(dataVec.data(), s_binVec.data() + byteOffset, totalBytes);
		}
	}

	template <typename T>
	void glTFLoader::add2BinaryVector(std::vector<unsigned char>& bin_vec, unsigned int bytes_per_component)
	{
		for (size_t i = 0; i < bin_vec.size(); i += bytes_per_component)
		{
			if (i + bytes_per_component > bin_vec.size()) {
				GE_CORE_ERROR("GLTFLoader: Buffer access out of bounds during index adjustment");
				break;
			}
			
			T* value = reinterpret_cast<T*>(&bin_vec[i]);
			*value += s_indexBuffer_offset;
		}
	}

	void glTFLoader::loadTransforms()
	{
		if (!s_glTFfile.contains("scenes") || s_glTFfile["scenes"].empty() || 
			!s_glTFfile["scenes"][0].contains("nodes") || s_glTFfile["scenes"][0]["nodes"].empty()) {
			GE_CORE_WARN("GLTFLoader: No scene or nodes to load transforms from");
			return;
		}
		
		glm::mat4 root_mat(1.0f);
		unsigned int root_node = s_glTFfile["scenes"][0]["nodes"][0];
		
		if (root_node >= s_nodes.size()) {
			GE_CORE_ERROR("GLTFLoader: Root node index out of range: {}", root_node);
			return;
		}
		
		traverseNode(root_mat, s_nodes[root_node]);
	}

	void glTFLoader::traverseNode(glm::mat4& transformMatrix, json& node)
	{
		// Default transform components
		glm::vec3 translationV(0.0f);
		glm::vec3 scaleV(1.0f);
		glm::quat rotationQ(1.0f, 0.0f, 0.0f, 0.0f);

		// Extract transform components if present
		if (node.contains("translation"))
			translationV = { 
				node["translation"][0], 
				node["translation"][1],
				node["translation"][2] 
			};
			
		if (node.contains("rotation")) // wxyz(gltf) -> xyzw(opengl)
			rotationQ = { 
				node["rotation"][3], 
				node["rotation"][0], 
				node["rotation"][1],
				node["rotation"][2] 
			};
			
		if (node.contains("scale"))
			scaleV = { 
				node["scale"][0], 
				node["scale"][1],
				node["scale"][2] 
			};

		// Calculate node's transform matrix
		glm::mat4 nodeMatrix = glm::mat4(1.0f);
		
		// Use matrix from file if provided
		if (node.contains("matrix")) {
			float node_values[16];
			for (int i = 0; i < 16; i++) {
				node_values[i] = node["matrix"][i];
			}
			nodeMatrix = glm::make_mat4(node_values);
		} else {
			// Otherwise build from components
			nodeMatrix = glm::translate(translationV) * 
						 glm::mat4_cast(rotationQ) * 
						 glm::scale(scaleV);
		}
		
		// Combine with parent transform
		nodeMatrix = transformMatrix * nodeMatrix;

		// Apply transform to mesh if this node references one
		if (node.contains("mesh")) {
			unsigned int meshIndex = node["mesh"];
			// Find the corresponding submesh and apply transform
			if (meshIndex < s_mesh->getSubMeshes().size()) {
				s_mesh->getSubMeshes()[meshIndex]->setTransform(nodeMatrix);
			}
		}

		// Recursively process children
		if (node.contains("children")) {
			for (auto& childIdx : node["children"]) {
				unsigned int childIndex = childIdx.get<unsigned int>();
				if (childIndex < s_nodes.size()) {
					traverseNode(nodeMatrix, s_nodes[childIndex]);
				} else {
					GE_CORE_WARN("GLTFLoader: Child node index out of range: {}", childIndex);
				}
			}
		}
	}

	void glTFLoader::cleanFiles()
	{
		s_glTFfile.clear();
		s_accessors.clear();
		s_meshes.clear();
		s_bufferViews.clear();
		s_buffers.clear();
		s_nodes.clear();
		s_materials.clear();
		s_animations.clear();
		s_skins.clear();
		
		s_binVec.clear();
		s_loaded_accessors.clear();
		s_prebuffer_layout.clear();
		s_buffer_layout = BufferLayout();
		
		s_ibo_count = 0;
		s_indexBuffer_offset = 0;
		s_vertexBuffer_offset = 0;
		s_prev_highest_index = 0;
		
		s_mesh = nullptr;
	}

}

*/