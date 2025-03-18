#pragma once

#include "Mesh.h"
#include <vector>

#include "../Buffers/Buffers.h"


#include "../File Loaders/glTF/glTFLoader.h"
//#include "../File Loaders/glTF/glTF2Loader.h"
#include "../Logger/Log.h"

namespace Rapture
{
	//class glTFLoader;

	Mesh::Mesh(std::string filepath)
	{

		m_VAO.reset(new VertexArray());

		//if (useGLTF2) {
		//	glTF2Loader::loadMesh(filepath, this);
		//} else {
			glTFLoader::loadMesh(filepath, this);
		//}

	}

	Mesh::~Mesh()
	{
		m_meshes = {};
	}

	std::shared_ptr<SubMesh> Mesh::addSubMesh()
	{
		std::shared_ptr<SubMesh> s = std::make_shared<SubMesh>(this);

		m_meshes.push_back(s);
		return s;
	}
}