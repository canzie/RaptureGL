#pragma once

#include "Mesh.h"
#include <vector>

#include "../Buffers/Buffers.h"
#include "../Buffers/VertexArray.h"

//#include "../File Loaders/glTF/glTFLoader.h"
//#include "../File Loaders/glTF/glTF2Loader.h"
#include "../Logger/Log.h"
#include <glad/glad.h>

namespace Rapture
{
	//class glTFLoader;

	Mesh::Mesh(std::string filepath)
	{

		//if (useGLTF2) {
		//	glTF2Loader::loadMesh(filepath, this);
		//} else {
			//glTFLoader::loadMesh(filepath, this);
		//}

	}

    

	Mesh::~Mesh()
	{
        BufferPoolManager& bufferPoolManager = BufferPoolManager::getInstance();
        bufferPoolManager.freeMeshData(m_meshBufferData);
	}

    bool Mesh::setMeshData(BufferLayout layout, const void* vertexData, size_t vertexDataSize, const void* indexData, size_t indexDataSize, size_t indexCount, unsigned int indexType)
    {

        BufferPoolManager& bufferPoolManager = BufferPoolManager::getInstance();
        m_meshBufferData = bufferPoolManager.allocateMeshData(layout, vertexData, vertexDataSize, indexData, indexDataSize, indexCount, indexType);

        if (m_meshBufferData.vao == nullptr) {
            GE_CORE_ERROR("Failed to allocate mesh data");
            return false;
        }

        //GE_CORE_INFO("========== Mesh::setMeshData {} ==========", m_meshBufferData.vao->getID());
        //bufferPoolManager.printBufferAllocations();
        //m_meshBufferData.vao->getBufferLayout().print();
        //GE_CORE_INFO("Mesh::setMeshData hash: {0}", m_meshBufferData.vao->getBufferLayout().hash());
        //m_meshBufferData.indexAllocation->print();
        //m_meshBufferData.vertexAllocation->print();
        //m_meshBufferData.print();
        return true;
    }

    std::shared_ptr<Mesh> Mesh::createCube(float size)
    {
        return nullptr;
	}
}