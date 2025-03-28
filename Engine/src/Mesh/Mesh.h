#pragma once


#include <string>
#include <vector>

#include "../Buffers/VertexArray.h"
#include "../Buffers/BufferPools.h"

namespace Rapture
{


	class Mesh
	{

	public:
		Mesh(std::string filepath);
        //Mesh(std::string filepath, bool useGLTF2=false);
		Mesh() = default;
		~Mesh();

		// setters


		// getters
		//std::shared_ptr<SubMesh> addSubMesh();

        bool setMeshData(BufferLayout layout, const void* vertexData, size_t vertexDataSize, const void* indexData, size_t indexDataSize, size_t indexCount, unsigned int indexType);

		// Create a simple cube mesh for testing
		static std::shared_ptr<Mesh> createCube(float size = 1.0f);

        size_t getIndexCount() { return m_indexCount; }
        size_t getOffsetBytes() { return m_offsetBytes; }

        MeshBufferData& getMeshData() { return m_meshBufferData; }


	private:
		// indices in the IBO that draw this sub mesh
		size_t m_indexCount;
		size_t m_offsetBytes;

        MeshBufferData m_meshBufferData;

	};



}