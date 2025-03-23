#pragma once

#include "SubMesh.h"

#include <string>
#include <vector>

namespace Rapture
{
	class SubMesh;


	class Mesh
	{

	public:
		Mesh(std::string filepath);
        //Mesh(std::string filepath, bool useGLTF2=false);
		Mesh();
		~Mesh();

		// setters
		void SetVAO(std::shared_ptr<VertexArray> VAO) { m_VAO = VAO; }


		// getters
		std::shared_ptr<VertexArray> getVAO() { return m_VAO; }
		//std::shared_ptr<SubMesh> addSubMesh();

		// Create a simple cube mesh for testing
		static std::shared_ptr<Mesh> createCube(float size = 1.0f);

        size_t getIndexCount() { return m_indexCount; }
        size_t getOffsetBytes() { return m_offsetBytes; }

        void setIndexCount(size_t indexCount) { m_indexCount = indexCount; }
        void setOffsetBytes(size_t offsetBytes) { m_offsetBytes = offsetBytes; }


	private:
		std::shared_ptr<VertexArray> m_VAO;
		// indices in the IBO that draw this sub mesh
		size_t m_indexCount;
		size_t m_offsetBytes;

	};



}