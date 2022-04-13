#ifndef GL_TRANSLATE_UTILS__H
#define GL_TRANSLATE_UTILS__H

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cubez/render_pipeline.h>

GLenum TranslateQbVertexAttribTypeToOpenGl(qbVertexAttribType type);
GLenum TranslateQbGpuBufferTypeToOpenGl(qbGpuBufferType type);
GLenum TranslateQbFilterTypeToOpenGl(qbFilterType type);
GLenum TranslateQbImageWrapTypeToOpenGl(qbImageWrapType type);
GLenum TranslateQbImageTypeToOpenGl(qbImageType type);
GLenum TranslateQbPixelFormatToInternalOpenGl(qbPixelFormat format);
GLenum TranslateQbPixelFormatToOpenGl(qbPixelFormat format);
GLenum TranslateQbPixelFormatToOpenGlSize(qbPixelFormat format);
size_t TranslateQbPixelFormatToPixelSize(qbPixelFormat format);
GLenum TranslateQbDrawModeToOpenGlMode(qbDrawMode mode);
GLenum TranslateQbFrontFaceToOpenGl(qbFrontFace face);
GLenum TranslateQbCullFaceToOpenGl(qbFace face);
GLenum TranslateQbRenderTestFuncToOpenGl(qbRenderTestFunc func);
GLenum TranslateQbBlendEquationToOpenGl(qbBlendEquation eq);
GLenum TranslateQbBlendFactorToOpenGl(qbBlendFactor factor);
GLenum TranslateQbPolygonModeToOpenGl(qbPolygonMode mode);
GLenum TranslateQbBufferAccessToOpenGl(qbBufferAccess access);

#endif  // GL_TRANSLATE_UTILS__H