#include "OpenGLRendererAPI.h"

#include <glad/glad.h>
#include "../Logger/Log.h"
#include "../Debug/TracyProfiler.h"

namespace Rapture {

	
	void OpenGLRendererAPI::setClearColor(const glm::vec4& color)
	{
		RAPTURE_PROFILE_FUNCTION();
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::clear()
	{
		RAPTURE_PROFILE_FUNCTION();
		RAPTURE_PROFILE_GPU_SCOPE("Clear Buffers");
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);  // Make sure depth test is using GL_LESS function
		glDepthMask(GL_TRUE);  // Ensure depth writing is enabled
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	/*
	void OpenGLRendererAPI::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
	{
		glViewport(x, y, width, height);
	}*/

	void OpenGLRendererAPI::drawIndexed(int indexCount, unsigned int comp_type)
	{
		drawIndexed(indexCount, comp_type, 0);
	}

	void OpenGLRendererAPI::drawIndexed(int indexCount, unsigned int comp_type, size_t offset, size_t vertexOffset)
	{
        // Clear any previous errors
        //while (glGetError() != GL_NO_ERROR);
        RAPTURE_PROFILE_GPU_SCOPE("GPU Draw Call");

        // Check index count
        if (indexCount <= 0) {
            GE_CORE_ERROR("Invalid index count: {0}", indexCount);
            return;
        }

        // Check component type
        if (comp_type != GL_UNSIGNED_BYTE && comp_type != GL_UNSIGNED_SHORT && comp_type != GL_UNSIGNED_INT) {
            GE_CORE_ERROR("Invalid component type: {0}", comp_type);
            return;
        }

        // Check if we have valid VAO and IBO bound
        GLint currentVAO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
        
        GLint currentIBO = 0;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentIBO);

        if (currentVAO == 0) {
            GE_CORE_ERROR("No VAO bound for draw call");
            return;
        }

        if (currentIBO == 0) {
            GE_CORE_ERROR("No IBO bound for draw call");
            return;
        }



		glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, (GLenum)comp_type, (void*)offset, vertexOffset);
        
        
	}

    void OpenGLRendererAPI::drawLine(glm::vec3 start, glm::vec3 end, glm::vec4 color)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("GPU Draw Line");

        // Create a Line object and call the overloaded function
    }

    
	
}