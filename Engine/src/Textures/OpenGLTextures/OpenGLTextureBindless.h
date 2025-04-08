#pragma once

#include <glad/glad.h>

// OpenGL Bindless Texture Extension definitions
// For ARB_bindless_texture extension
#ifndef GL_ARB_bindless_texture
#define GL_ARB_bindless_texture 1

typedef uint64_t GLuint64;

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations
extern GLuint64 glGetTextureHandleARB(GLuint texture);
extern GLuint64 glGetTextureSamplerHandleARB(GLuint texture, GLuint sampler);
extern void glMakeTextureHandleResidentARB(GLuint64 handle);
extern void glMakeTextureHandleNonResidentARB(GLuint64 handle);
extern GLuint64 glGetImageHandleARB(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
extern void glMakeImageHandleResidentARB(GLuint64 handle, GLenum access);
extern void glMakeImageHandleNonResidentARB(GLuint64 handle);
extern void glUniformHandleui64ARB(GLint location, GLuint64 value);
extern void glUniformHandleui64vARB(GLint location, GLsizei count, const GLuint64* value);
extern void glProgramUniformHandleui64ARB(GLuint program, GLint location, GLuint64 value);
extern void glProgramUniformHandleui64vARB(GLuint program, GLint location, GLsizei count, const GLuint64* values);
extern GLboolean glIsTextureHandleResidentARB(GLuint64 handle);
extern GLboolean glIsImageHandleResidentARB(GLuint64 handle);

#ifdef __cplusplus
}
#endif

#endif /* GL_ARB_bindless_texture */ 