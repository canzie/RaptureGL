#pragma once

#include "SubMesh.h"

#include <string>
#include <vector>

namespace Rapture
{
	class SubMesh;


	class Mesh : public std::enable_shared_from_this<Mesh>
	{

	public:
		Mesh(std::string filepath);
        //Mesh(std::string filepath, bool useGLTF2=false);
		Mesh() = default;
		~Mesh();

		// setters
		//void SetVAO(std::shared_ptr<VertexArray> VAO) { m_VAO = VAO; }


		// getters
		std::shared_ptr<VertexArray> getVAO() { return m_VAO; }
		std::vector<std::shared_ptr<SubMesh>>& getSubMeshes() { return m_meshes; }
		std::shared_ptr<SubMesh> addSubMesh();

	private:
		std::vector<std::shared_ptr<SubMesh>> m_meshes;
		std::shared_ptr<VertexArray> m_VAO;

	};



}