GLenum TranslateQbVertexAttribTypeToOpenGl(qbVertexAttribType type) {
  switch (type) {
    case QB_VERTEX_ATTRIB_TYPE_BYTE: return GL_BYTE;
    case QB_VERTEX_ATTRIB_TYPE_UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
    case QB_VERTEX_ATTRIB_TYPE_SHORT: return GL_SHORT;
    case QB_VERTEX_ATTRIB_TYPE_UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
    case QB_VERTEX_ATTRIB_TYPE_INT: return GL_INT;
    case QB_VERTEX_ATTRIB_TYPE_UNSIGNED_INT: return GL_UNSIGNED_INT;
    case QB_VERTEX_ATTRIB_TYPE_HALF_FLOAT: return GL_HALF_FLOAT;
    case QB_VERTEX_ATTRIB_TYPE_FLOAT: return GL_FLOAT;
    case QB_VERTEX_ATTRIB_TYPE_DOUBLE: return GL_DOUBLE;
    case QB_VERTEX_ATTRIB_TYPE_FIXED: return GL_FIXED;
  }
  assert(false && "Invalid qbVertexAttribType");
  return 0;
}

GLenum TranslateQbGpuBufferTypeToOpenGl(qbGpuBufferType type) {
  switch (type) {
    case QB_GPU_BUFFER_TYPE_VERTEX: return GL_ARRAY_BUFFER;
    case QB_GPU_BUFFER_TYPE_INDEX: return GL_ELEMENT_ARRAY_BUFFER;
    case QB_GPU_BUFFER_TYPE_UNIFORM: return GL_UNIFORM_BUFFER;
  }
  assert(false && "Invalid qbGpuBufferType");
  return 0;
}

GLenum TranslateQbFilterTypeToOpenGl(qbFilterType type) {
  switch (type) {
    case QB_FILTER_TYPE_NEAREST: return GL_NEAREST;
    case QB_FILTER_TYPE_LINEAR: return GL_LINEAR;
    case QB_FILTER_TYPE_NEAREST_MIPMAP_NEAREST: return GL_NEAREST_MIPMAP_NEAREST;
    case QB_FILTER_TYPE_LINEAR_MIPMAP_NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
    case QB_FILTER_TYPE_NEAREST_MIPMAP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
    case QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
  }
  assert(false && "Invalid qbFilterType");
  return 0;
}
GLenum TranslateQbImageWrapTypeToOpenGl(qbImageWrapType type) {
  switch (type) {
    case QB_IMAGE_WRAP_TYPE_REPEAT: return GL_REPEAT;
    case QB_IMAGE_WRAP_TYPE_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    case QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
    case QB_IMAGE_WRAP_TYPE_CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;
  }
  assert(false && "Invalid qbImageWrapType");
  return 0;
}

GLenum TranslateQbImageTypeToOpenGl(qbImageType type) {
  switch (type) {
    case QB_IMAGE_TYPE_1D: return GL_TEXTURE_1D;
    case QB_IMAGE_TYPE_2D: return GL_TEXTURE_2D;
    case QB_IMAGE_TYPE_3D: return GL_TEXTURE_3D;
    case QB_IMAGE_TYPE_CUBE: return GL_TEXTURE_CUBE_MAP;
    case QB_IMAGE_TYPE_1D_ARRAY: return GL_TEXTURE_1D_ARRAY;
    case QB_IMAGE_TYPE_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
    case QB_IMAGE_TYPE_CUBE_ARRAY: return GL_TEXTURE_CUBE_MAP_ARRAY;
  }
  assert(false && "Invalid qbImageType");
  return 0;
}

GLenum TranslateQbPixelFormatToInternalOpenGl(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8: return GL_R8;
    case QB_PIXEL_FORMAT_RG8: return GL_RGBA8;
    case QB_PIXEL_FORMAT_RGB8: return GL_RGBA8;
    case QB_PIXEL_FORMAT_RGBA8: return GL_RGBA8;
    case QB_PIXEL_FORMAT_R16F: return GL_R16F;
    case QB_PIXEL_FORMAT_RG16F: return GL_RG16F;
    case QB_PIXEL_FORMAT_RGB16F: return GL_RGB16F;
    case QB_PIXEL_FORMAT_RGBA16F: return GL_RGBA16F;
    case QB_PIXEL_FORMAT_R32F: return GL_R32F;
    case QB_PIXEL_FORMAT_RG32F: return GL_RG32F;
    case QB_PIXEL_FORMAT_RGB32F: return GL_RGB32F;
    case QB_PIXEL_FORMAT_RGBA32F: return GL_RGBA32F;
    case QB_PIXEL_FORMAT_D32: return GL_DEPTH_COMPONENT24;
    case QB_PIXEL_FORMAT_D24_S8: return GL_DEPTH24_STENCIL8;
    case QB_PIXEL_FORMAT_S8: return GL_STENCIL_INDEX;
  }
  assert(false && "Invalid qbPixelFormat");
  return 0;
}

GLenum TranslateQbPixelFormatToOpenGl(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8: return GL_RED;
    case QB_PIXEL_FORMAT_RG8: return GL_RG;
    case QB_PIXEL_FORMAT_RGB8: return GL_RGB;
    case QB_PIXEL_FORMAT_RGBA8: return GL_RGBA;
    case QB_PIXEL_FORMAT_R16F: return GL_RED;
    case QB_PIXEL_FORMAT_RG16F: return GL_RG;
    case QB_PIXEL_FORMAT_RGB16F: return GL_RGB;
    case QB_PIXEL_FORMAT_RGBA16F: return GL_RGBA;
    case QB_PIXEL_FORMAT_R32F: return GL_RED;
    case QB_PIXEL_FORMAT_RG32F: return GL_RG;
    case QB_PIXEL_FORMAT_RGB32F: return GL_RGB;
    case QB_PIXEL_FORMAT_RGBA32F: return GL_RGBA;
    case QB_PIXEL_FORMAT_D32: return GL_DEPTH_COMPONENT;
    case QB_PIXEL_FORMAT_D24_S8: return GL_DEPTH_STENCIL;
    case QB_PIXEL_FORMAT_S8: return GL_STENCIL_INDEX;
  }
  assert(false && "Invalid qbPixelFormat");
  return 0;
}

GLenum TranslateQbPixelFormatToOpenGlSize(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8:
    case QB_PIXEL_FORMAT_RG8:
    case QB_PIXEL_FORMAT_RGB8:
    case QB_PIXEL_FORMAT_RGBA8: return GL_UNSIGNED_BYTE;
    case QB_PIXEL_FORMAT_R16F:
    case QB_PIXEL_FORMAT_RG16F:
    case QB_PIXEL_FORMAT_RGB16F:
    case QB_PIXEL_FORMAT_RGBA16F: return GL_HALF_FLOAT;
    case QB_PIXEL_FORMAT_R32F:
    case QB_PIXEL_FORMAT_RG32F:
    case QB_PIXEL_FORMAT_RGB32F:
    case QB_PIXEL_FORMAT_RGBA32F: return GL_FLOAT;
    case QB_PIXEL_FORMAT_D32: return GL_FLOAT;
    case QB_PIXEL_FORMAT_D24_S8: return GL_UNSIGNED_INT_24_8;
    case QB_PIXEL_FORMAT_S8: return GL_UNSIGNED_BYTE;
  }
  assert(false && "Invalid qbPixelFormat");
  return 0;
}

size_t TranslateQbPixelFormatToPixelSize(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8: return 1;
    case QB_PIXEL_FORMAT_RG8: return 2;
    case QB_PIXEL_FORMAT_RGB8: return 3;
    case QB_PIXEL_FORMAT_RGBA8: return 4;
    case QB_PIXEL_FORMAT_R16F: return 2;
    case QB_PIXEL_FORMAT_RG16F: return 4;
    case QB_PIXEL_FORMAT_RGB16F: return 6;
    case QB_PIXEL_FORMAT_RGBA16F: return 8;
    case QB_PIXEL_FORMAT_R32F: return 4;
    case QB_PIXEL_FORMAT_RG32F: return 8;
    case QB_PIXEL_FORMAT_RGB32F: return 12;
    case QB_PIXEL_FORMAT_RGBA32F: return 16;
    case QB_PIXEL_FORMAT_D32: return 4;
    case QB_PIXEL_FORMAT_D24_S8: return 4;
    case QB_PIXEL_FORMAT_S8: return 1;
  }
  assert(false && "Invalid qbPixelFormat");
  return 0;
}

GLenum TranslateQbDrawModeToOpenGlMode(qbDrawMode mode) {
  switch (mode) {
    case QB_DRAW_MODE_POINT_LIST: return GL_POINTS;
    case QB_DRAW_MODE_LINE_LIST: return GL_LINES;
    case QB_DRAW_MODE_LINE_STRIP: return GL_LINE_STRIP;
    case QB_DRAW_MODE_TRIANGLE_LIST: return GL_TRIANGLES;
    case QB_DRAW_MODE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case QB_DRAW_MODE_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
  }
  assert(false && "Invalid qbDrawMode");
  return 0;
}

GLenum TranslateQbFrontFaceToOpenGl(qbFrontFace face) {
  switch (face) {
    case QB_FRONT_FACE_CW: return GL_CW;
    case QB_FRONT_FACE_CCW: return GL_CCW;
  };
  assert(false && "Invalid qbFrontFace");
  return 0;
}

GLenum TranslateQbCullFaceToOpenGl(qbFace face) {
  switch (face) {
    case QB_FACE_NONE: return GL_NONE;
    case QB_FACE_FRONT: return GL_FRONT;
    case QB_FACE_BACK:  return GL_BACK;
    case QB_FACE_FRONT_AND_BACK: return GL_FRONT_AND_BACK;
  }
  assert(false && "Invalid qbFace");
  return GL_NONE;
}

GLenum TranslateQbRenderTestFuncToOpenGl(qbRenderTestFunc func) {
  switch (func) {
    case QB_RENDER_TEST_FUNC_NEVER: return GL_NEVER;
    case QB_RENDER_TEST_FUNC_LESS: return GL_LESS;
    case QB_RENDER_TEST_FUNC_EQUAL: return GL_EQUAL;
    case QB_RENDER_TEST_FUNC_LEQUAL: return GL_LEQUAL;
    case QB_RENDER_TEST_FUNC_GREATER: return GL_GREATER;
    case QB_RENDER_TEST_FUNC_NOTEQUAL: return GL_NOTEQUAL;
    case QB_RENDER_TEST_FUNC_GEQUAL: return GL_GEQUAL;
    case QB_RENDER_TEST_FUNC_ALWAYS: return GL_ALWAYS;
  }
  assert(false && "Invalid qbRenderTestFunc");
  return GL_NEVER;
}

GLenum TranslateQbBlendEquationToOpenGl(qbBlendEquation eq) {
  switch (eq) {
    case QB_BLEND_EQUATION_ADD: return GL_FUNC_ADD;
    case QB_BLEND_EQUATION_SUBTRACT: return GL_FUNC_SUBTRACT;
    case QB_BLEND_EQUATION_REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
    case QB_BLEND_EQUATION_MIN: return GL_MIN;
    case QB_BLEND_EQUATION_MAX: return GL_MAX;
  }
  assert(false && "Invalid qbBlendEquation");
  return 0;
}

GLenum TranslateQbBlendFactorToOpenGl(qbBlendFactor factor) {
  switch (factor) {
    case QB_BLEND_FACTOR_ZERO: return GL_ZERO;
    case QB_BLEND_FACTOR_ONE: return GL_ONE;
    case QB_BLEND_FACTOR_SRC_COLOR: return GL_SRC_COLOR;
    case QB_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
    case QB_BLEND_FACTOR_DST_COLOR: return GL_DST_COLOR;
    case QB_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
    case QB_BLEND_FACTOR_SRC_ALPHA: return GL_SRC_ALPHA;
    case QB_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC1_ALPHA;
    case QB_BLEND_FACTOR_DST_ALPHA: return GL_DST_ALPHA;
    case QB_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
    case QB_BLEND_FACTOR_CONSTANT_COLOR: return GL_CONSTANT_COLOR;
    case QB_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
    case QB_BLEND_FACTOR_CONSTANT_ALPHA: return GL_CONSTANT_ALPHA;
    case QB_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;
    case QB_BLEND_FACTOR_SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
  }
  assert(false && "Invalid qbBlendFactor");
  return GL_ZERO;
}

GLenum TranslateQbPolygonModeToOpenGl(qbPolygonMode mode) {
  switch (mode) {
    case QB_POLYGON_MODE_POINT: return GL_POINT;
    case QB_POLYGON_MODE_LINE: return GL_LINE;
    case QB_POLYGON_MODE_FILL: return GL_FILL;
  }
  assert(false && "Invalid qbPolygonMode");
  return 0;
}