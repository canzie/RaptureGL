#include "OpenGLFormatConverters.h"

#include "../../Logger/Log.h"

namespace Rapture {

	GLenum TextureFormatToGL(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::RGBA8:       return GL_RGBA8;
			case TextureFormat::RGB8:        return GL_RGB8;
			case TextureFormat::RGB16F:      return GL_RGB16F;
			case TextureFormat::RGB32F:      return GL_RGB32F;
			case TextureFormat::RGBA16F:     return GL_RGBA16F;
			case TextureFormat::RGBA32F:     return GL_RGBA32F;
			case TextureFormat::R11G11B10F:  return GL_R11F_G11F_B10F;
			case TextureFormat::RG16F:      return GL_RG16F;
		}

		GE_CORE_ERROR("TextureFormatToGL - Unknown framebuffer texture format!");
		return 0;
	}
	
	GLenum TextureFormatToGLDataFormat(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::RGBA8:       return GL_RGBA;
			case TextureFormat::RGB8:        return GL_RGB;
			case TextureFormat::RGB16F:      return GL_RGB;
			case TextureFormat::RGB32F:      return GL_RGB;
			case TextureFormat::RGBA16F:     return GL_RGBA;
			case TextureFormat::RGBA32F:     return GL_RGBA;
			case TextureFormat::R11G11B10F:  return GL_RGB;
			case TextureFormat::RG16F:      return GL_RG;
		}

		GE_CORE_ERROR("TextureFormatToGLDataFormat - Unknown framebuffer data format!");
		return 0;
	}
	
	GLenum TextureFormatToGLDataType(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::RGBA8:       return GL_UNSIGNED_BYTE;
			case TextureFormat::RGB8:        return GL_UNSIGNED_BYTE;
			case TextureFormat::RGB16F:      return GL_FLOAT;
			case TextureFormat::RGB32F:      return GL_FLOAT;
			case TextureFormat::RGBA16F:     return GL_FLOAT;
			case TextureFormat::RGBA32F:     return GL_FLOAT;
			case TextureFormat::R11G11B10F:  return GL_FLOAT;
			case TextureFormat::RG16F:      return GL_FLOAT;
		}

		GE_CORE_ERROR("TextureFormatToGLDataType - Unknown framebuffer data type!");
		return GL_UNSIGNED_BYTE;
	}

    GLenum convertFilterToGL(TextureFilter filter)
    {
        switch (filter) {
            case TextureFilter::Nearest:              return GL_NEAREST;
            case TextureFilter::Linear:               return GL_LINEAR;
            case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
            case TextureFilter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
            case TextureFilter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
            case TextureFilter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
            default:
                GE_CORE_WARN("convertFilterToGL - Unknown filter type, defaulting to Linear");
                return GL_LINEAR;
        }
    }

    GLenum convertWrapToGL(TextureWrap wrap)
    {
        switch (wrap) {
            case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
            case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
            case TextureWrap::Repeat:         return GL_REPEAT;
            default:
                GE_CORE_WARN("convertWrapToGL - Unknown wrap type, defaulting to Repeat");
                return GL_REPEAT;
        }
    }

} 