#pragma once

#include <memory>
#include "../Buffers/VertexArray.h"
#include <glm/gtx/transform.hpp>

#include "Mesh.h"

namespace Rapture
{
	class Mesh;



	class SubMesh
	{
	public:
		SubMesh() = default;

		SubMesh(Mesh* parent) {

			m_parentMesh = parent;

			m_transform = glm::mat4(1.0f);
			m_name = "Untitled";
			m_indexCount = 0;
			m_offsetBytes = 0;
		}

		//std::shared_ptr<VertexArray> getVAO() const { return m_VAO; }

		glm::mat4 getTransform() { return m_transform; }
		void setTransform(glm::mat4& transform_mat) { m_transform = transform_mat; }


		size_t getOffset() { return m_offsetBytes; }
		size_t getIndexCount() { return m_indexCount; }
		void setPartition(size_t index_count, size_t offset)
		{
			//these are += and not just = because gltf dooki. setPartition(n, 0), will add another submesh to this.
			m_indexCount += index_count;
			m_offsetBytes += offset;
		}

		Mesh* getParentMesh() { return m_parentMesh; }



		std::string m_name;

	private:
		glm::mat4 m_transform;

		Mesh* m_parentMesh;

		// indices in the IBO that draw this sub mesh
		size_t m_indexCount;
		size_t m_offsetBytes;

	};

}