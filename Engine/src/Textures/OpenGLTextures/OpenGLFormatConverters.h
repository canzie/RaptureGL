#pragma once

#include "../Texture.h"


#include <glad/glad.h>


namespace Rapture {

	GLenum TextureFormatToGL(TextureFormat format);
	
	GLenum TextureFormatToGLDataFormat(TextureFormat format);
	
	GLenum TextureFormatToGLDataType(TextureFormat format);

    GLenum convertFilterToGL(TextureFilter filter);

    GLenum convertWrapToGL(TextureWrap wrap);

}
