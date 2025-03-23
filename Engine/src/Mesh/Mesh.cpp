#pragma once

#include "Mesh.h"
#include <vector>

#include "../Buffers/Buffers.h"
#include "../Buffers/VertexArray.h"
#include "../Buffers/BufferLayout.h"

//#include "../File Loaders/glTF/glTFLoader.h"
//#include "../File Loaders/glTF/glTF2Loader.h"
#include "../Logger/Log.h"
#include <glad/glad.h>

namespace Rapture
{
	//class glTFLoader;

	Mesh::Mesh(std::string filepath)
	{

		m_VAO.reset(new VertexArray());

		//if (useGLTF2) {
		//	glTF2Loader::loadMesh(filepath, this);
		//} else {
			//glTFLoader::loadMesh(filepath, this);
		//}

	}

    Mesh::Mesh()
    : m_indexCount(0), m_offsetBytes(0) {}

    

	Mesh::~Mesh()
	{
	}



	std::shared_ptr<Mesh> Mesh::createCube(float size)
	{
		GE_CORE_INFO("Creating test cube mesh with size {0}", size);
		
		std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
		mesh->m_VAO = std::make_shared<VertexArray>();
		
		float halfSize = size / 2.0f;
		
		// Define vertex data for a cube in non-interleaved format (VVVVNNNNCCCC)
		// First all positions, then all normals, then all texture coordinates
		
		// Positions (24 vertices, 3 floats each: x,y,z)
		float positions[] = {
			// Front face (positive Z)
			-halfSize, -halfSize,  halfSize,
			 halfSize, -halfSize,  halfSize,
			 halfSize,  halfSize,  halfSize,
			-halfSize,  halfSize,  halfSize,
			
			// Back face (negative Z)
			 halfSize, -halfSize, -halfSize,
			-halfSize, -halfSize, -halfSize,
			-halfSize,  halfSize, -halfSize,
			 halfSize,  halfSize, -halfSize,
			
			// Top face (positive Y)
			-halfSize,  halfSize,  halfSize,
			 halfSize,  halfSize,  halfSize,
			 halfSize,  halfSize, -halfSize,
			-halfSize,  halfSize, -halfSize,
			
			// Bottom face (negative Y)
			-halfSize, -halfSize, -halfSize,
			 halfSize, -halfSize, -halfSize,
			 halfSize, -halfSize,  halfSize,
			-halfSize, -halfSize,  halfSize,
			
			// Right face (positive X)
			 halfSize, -halfSize,  halfSize,
			 halfSize,  halfSize,  halfSize,
			 halfSize,  halfSize, -halfSize,
			 halfSize, -halfSize, -halfSize,
			
			// Left face (negative X)
			-halfSize, -halfSize, -halfSize,
			-halfSize, -halfSize,  halfSize,
			-halfSize,  halfSize,  halfSize,
			-halfSize,  halfSize, -halfSize
		};
		
		// Normals (24 vertices, 3 floats each: nx,ny,nz)
		float normals[] = {
			// Front face (positive Z)
			0.0f,  0.0f,  1.0f,
			0.0f,  0.0f,  1.0f,
			0.0f,  0.0f,  1.0f,
			0.0f,  0.0f,  1.0f,
			
			// Back face (negative Z)
			0.0f,  0.0f, -1.0f,
			0.0f,  0.0f, -1.0f,
			0.0f,  0.0f, -1.0f,
			0.0f,  0.0f, -1.0f,
			
			// Top face (positive Y)
			0.0f,  1.0f,  0.0f,
			0.0f,  1.0f,  0.0f,
			0.0f,  1.0f,  0.0f,
			0.0f,  1.0f,  0.0f,
			
			// Bottom face (negative Y)
			0.0f, -1.0f,  0.0f,
			0.0f, -1.0f,  0.0f,
			0.0f, -1.0f,  0.0f,
			0.0f, -1.0f,  0.0f,
			
			// Right face (positive X)
			1.0f,  0.0f,  0.0f,
			1.0f,  0.0f,  0.0f,
			1.0f,  0.0f,  0.0f,
			1.0f,  0.0f,  0.0f,
			
			// Left face (negative X)
			-1.0f,  0.0f,  0.0f,
			-1.0f,  0.0f,  0.0f,
			-1.0f,  0.0f,  0.0f,
			-1.0f,  0.0f,  0.0f
		};
		
		// Texture coordinates (24 vertices, 2 floats each: u,v)
		float texcoords[] = {
			// Front face
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			
			// Back face
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			
			// Top face
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			
			// Bottom face
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f,
			
			// Right face
			0.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			
			// Left face
			0.0f, 0.0f,
			1.0f, 0.0f,
			1.0f, 1.0f,
			0.0f, 1.0f
		};
		
		// Define indices for the cube (6 faces, 2 triangles each, 3 vertices per triangle)
		// All faces use CCW winding when viewed from outside
		unsigned int indices[] = {
			0, 1, 2,    0, 2, 3,    // Front face
			4, 5, 6,    4, 6, 7,    // Back face
			8, 9, 10,   8, 10, 11,  // Top face
			12, 13, 14, 12, 14, 15, // Bottom face
			16, 17, 18, 16, 18, 19, // Right face
			20, 21, 22, 20, 22, 23  // Left face
		};
		
		// Combine all vertex data into a single buffer in VVVVNNNNCCCC format
		size_t posSize = sizeof(positions);
		size_t normSize = sizeof(normals);
		size_t texSize = sizeof(texcoords);
		size_t totalSize = posSize + normSize + texSize;
		
		std::vector<unsigned char> combinedVertexData(totalSize);
		
		// Copy position data to the beginning of the buffer
		memcpy(combinedVertexData.data(), positions, posSize);
		
		// Copy normal data after the positions
		memcpy(combinedVertexData.data() + posSize, normals, normSize);
		
		// Copy texcoord data after the normals
		memcpy(combinedVertexData.data() + posSize + normSize, texcoords, texSize);
		
		// Create vertex buffer
		std::shared_ptr<VertexBuffer> vertexBuffer = std::make_shared<VertexBuffer>(combinedVertexData, BufferUsage::Static);
		if (!vertexBuffer) {
			GE_CORE_ERROR("Failed to create vertex buffer for cube");
			return nullptr;
		}
		
		// Set vertex buffer
		mesh->m_VAO->setVertexBuffer(vertexBuffer);

        combinedVertexData.clear();
		
		// Set up buffer layout for non-interleaved format
		BufferLayout layout;
		
		// Position attribute (offset at start of buffer)
		BufferAttribute posAttrib;
		posAttrib.name = "POSITION";
		posAttrib.componentType = GL_FLOAT;
		posAttrib.type = "VEC3";
		posAttrib.offset = 0;
		layout.buffer_attribs.push_back(posAttrib);
		
		// Normal attribute (offset after positions)
		BufferAttribute normalAttrib;
		normalAttrib.name = "NORMAL";
		normalAttrib.componentType = GL_FLOAT;
		normalAttrib.type = "VEC3";
		normalAttrib.offset = 3 * sizeof(float);
		layout.buffer_attribs.push_back(normalAttrib);
		
		// Texture coordinate attribute (offset after normals)
		BufferAttribute texCoordAttrib;
		texCoordAttrib.name = "TEXCOORD_0";
		texCoordAttrib.componentType = GL_FLOAT;
		texCoordAttrib.type = "VEC2";
		texCoordAttrib.offset = 6 * sizeof(float);
		layout.buffer_attribs.push_back(texCoordAttrib);
		
		// Set buffer layout
		mesh->m_VAO->setBufferLayout(layout);
		
		// Create index buffer
		size_t indexDataSize = sizeof(indices);
		std::shared_ptr<IndexBuffer> indexBuffer = std::make_shared<IndexBuffer>(indexDataSize, GL_UNSIGNED_INT, BufferUsage::Static, indices);
		if (!indexBuffer) {
			GE_CORE_ERROR("Failed to create index buffer for cube");
			return nullptr;
		}
		
		// Set index buffer
		mesh->m_VAO->setIndexBuffer(indexBuffer);
		
		// Create a single submesh for the cube
		
		// Use setPartition for GLTF compatibility
		//submesh->setPartition(36, 0); // 6 faces * 2 triangles * 3 vertices
		mesh->setIndexCount(36);
		mesh->setOffsetBytes(0);

		GE_CORE_INFO("Created cube mesh with {0} vertices, {1} indices", 24, 36);
		GE_CORE_INFO("SubMesh index count: {0}, offset: {1}", mesh->getIndexCount(), mesh->getOffsetBytes());
		
		return mesh;
	}
}