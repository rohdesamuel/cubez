#include <cubez/memory.h>
#include <cubez/sprite.h>
#include <cubez/render_pipeline.h>
#include <cglm/struct.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <stdlib.h>

#include "sprite_internal.h"
#include "inline_shaders.h"

constexpr int MAX_BATCH_TEXTURE_UNITS = 31;
constexpr size_t SPRITE_VERTEX_ATTRIBUTE_SIZE =
  // Vertex
  2 +

  // Offset
  2 +

  // Color
  4 +

  // Scale
  2 +

  // Texture ix/iy
  2 +

  // Rotation
  1 + 

  // Texture Id
  1;
constexpr size_t MAX_NUM_SPRITES_PER_BATCH = 1000;
constexpr uint32_t TEXTURE_UNITS_START_BINDING = 1;

std::filesystem::path sprite_path;

namespace
{

std::string get_stem(const std::filesystem::path &p) {
  return (p.stem().string());
}

}  // namespace

// std140 layout
struct UniformCamera {
  alignas(16) mat4s projection;

  static uint32_t Binding() {
    return 0;
  }
};

enum RenderMode {
  GUI_RENDER_MODE_SOLID,
  GUI_RENDER_MODE_IMAGE,
  GUI_RENDER_MODE_STRING,
};

struct QueuedSprite {
  //qbSprite sprite;
  float depth;
  size_t index;

  // Position to draw at.
  vec2s pos;

  // Scale of sprite.
  vec2s scale;

  // Rotation of sprite in radians.
  float rot;

  // RGBA color.
  vec4s col;

  int32_t left, top;

  int32_t w, h;

  // If the sprite is part of an animation, then this will point to its
  // animator.
  //qbSprite animator_sprite;
  qbSpriteAnimator animator;

  qbImage sprite_img;

  uint32_t sprite_w, sprite_h;

  float offset_x, offset_y;

  bool operator<(const QueuedSprite& other) {
    if (depth == other.depth) {
      return index > other.index;
    }
    return depth < other.depth;
  }
};

typedef struct qbSpriteAnimation_ {
  std::vector<qbSprite> frames;
  std::vector<double> durations;

  double frame_duration;
  bool repeat;
  int keyframe;

  vec2s offset;

  uint32_t w, h;
} qbSpriteAnimation_, *qbSpriteAnimation;

struct Batch {
  std::set<qbImage> images;

  std::vector<QueuedSprite> sprites;
};

std::vector<QueuedSprite> sprites;

qbRenderPass sprite_render_pass;

qbShaderResourcePipelineLayout sprite_render_pipeline_layout;
qbRenderPipeline sprite_render_pipeline;
qbShaderResourceLayout sprite_resource_layout;

qbRenderGroup batched_sprites;
qbMeshBuffer sprite_quads;

// A 1x1 white texture that is used to render solid color sprites.
qbImage clear_texture;

qbComponent sprite_component;

std::array<std::array<qbShaderResourceSet, MAX_BATCH_TEXTURE_UNITS>, 2> sprite_textures;

qbSprite qb_spritesheet_load(const utf8_t* filename, int tw, int th, int margin) {
  qbSprite sheet = qb_sprite_load(filename);
  sheet->tw = tw;
  sheet->th = th;
  sheet->margin = margin;

  return sheet;
}

qbSprite qb_sprite_fromsheet(qbSprite sheet, int ix, int iy) {
  qbSprite ret = new qbSprite_{};
  ret->w = sheet->w;
  ret->h = sheet->h;
  ret->img = sheet->img;
  ret->margin = sheet->margin;
  ret->tw = sheet->tw;
  ret->th = sheet->th;
  ret->ix = ix;
  ret->iy = iy;
  ret->offset = sheet->offset;

  return ret;
}

qbSprite qb_sprite_load(const utf8_t* filename) {
  qbImage img{};
  qbImageAttr_ attr{};
  attr.type = qbImageType::QB_IMAGE_TYPE_2D;

  qb_image_load(&img, &attr, filename);

  qbSprite ret = new qbSprite_{};
  ret->w = qb_image_width(img);
  ret->h = qb_image_height(img);
  ret->tw = ret->w;
  ret->th = ret->h;
  ret->img = img;

  return ret;
}

qbSprite qb_sprite_frompixels(qbPixelMap pixels) {
  qbImage img{};
  qbImageAttr_ attr{};
  attr.type = qbImageType::QB_IMAGE_TYPE_2D;

  qb_image_create(&img, &attr, pixels);

  qbSprite ret = new qbSprite_{};
  ret->w = qb_image_width(img);
  ret->h = qb_image_height(img);
  ret->tw = ret->w;
  ret->th = ret->h;
  ret->img = img;

  return ret;
}

qbSprite qb_sprite_fromimage(qbImage image) {
  qbSprite ret = new qbSprite_{};
  ret->w = qb_image_width(image);
  ret->h = qb_image_height(image);
  ret->tw = ret->w;
  ret->th = ret->h;
  ret->img = image;

  return ret;
}

qbSpriteAnimation qb_spriteanimation_loaddir(const utf8_t* dir, qbSpriteAnimationAttr attr) {
  std::filesystem::path dir_path(dir);

  std::vector<qbSprite> frames;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    std::filesystem::path sprite_path = dir_path / entry.path().filename();
    frames.push_back(qb_sprite_load(sprite_path.u8string().c_str()));
  }

  std::vector<double> durations;
  for (int i = 0; i < attr->frame_count; ++i) {
    durations.push_back(attr->durations[i]);
  }

  if (attr->frame_count > frames.size()) {
    for (int i = 0; i < frames.size() - attr->frame_count; ++i) {
      durations.push_back(0.0);
    }
  }

  attr->frames = frames.data();
  attr->durations = durations.data();
  attr->frame_count = frames.size();

  return qb_spriteanimation_create(attr);
}

void qb_sprite_draw_internal(qbSprite sprite, vec2s pos, vec2s scale, float rot,
                             vec4s col, int32_t left, int32_t top, int32_t width, int32_t height) {
  qbSprite animator_sprite = nullptr;
  qbSpriteAnimator animator = nullptr;

  if (sprite->animator->animation) {
    animator_sprite = sprite;
    animator = sprite->animator;
    sprite = sprite->animator->animation->frames[sprite->animator->frame];    
  }

  float offset_x = animator ? animator_sprite->offset.x : sprite->offset.x;
  float offset_y = animator ? animator_sprite->offset.y : sprite->offset.y;

  sprites.push_back({
    .depth = sprite->depth,
    .index = sprites.size(),
    .pos = pos,
    .scale = scale,
    .rot = rot,
    .col = col,
    .left = left,
    .top = top,
    .w = width,
    .h = height,
    .animator = animator,
    .sprite_img = sprite->img,
    .sprite_w = sprite->w, .sprite_h = sprite->h,
    .offset_x = offset_x, .offset_y = offset_y,
    });
  std::push_heap(sprites.begin(), sprites.end());
}

void qb_sprite_draw_internal(qbSprite sprite, vec2s pos, vec2s scale, float rot, vec4s col) {
  qbSprite animator_sprite = nullptr;
  qbSpriteAnimator animator = nullptr;

  if (sprite->animator && sprite->animator->animation) {
    animator_sprite = sprite;
    animator = sprite->animator;
    sprite = sprite->animator->animation->frames[sprite->animator->frame];
  }

  int32_t width = sprite->tw + sprite->margin;
  int32_t height = sprite->th + sprite->margin;
  int32_t left = sprite->ix * width;
  int32_t top = sprite->iy * height;
  float offset_x = animator ? animator_sprite->offset.x : sprite->offset.x;
  float offset_y = animator ? animator_sprite->offset.y : sprite->offset.y;

  sprites.push_back({
    .depth = sprite->depth,
    .index = sprites.size(),
    .pos = pos,
    .scale = scale,
    .rot = rot,
    .col = col,
    .left = left,
    .top = top,
    .w = width,
    .h = height,
    .animator = animator,
    .sprite_img = sprite->img,
    .sprite_w = sprite->w, .sprite_h = sprite->h,
    .offset_x = offset_x, .offset_y = offset_y,
    });

  std::push_heap(sprites.begin(), sprites.end());
}

void qb_sprite_draw(qbSprite sprite, vec2s pos) {
  qb_sprite_draw_internal(sprite, pos, GLMS_VEC2_ONE_INIT, 0.f, GLMS_VEC4_ONE_INIT);
}

void qb_sprite_draw_ext(qbSprite sprite, vec2s pos, vec2s scale, float rot, vec4s col) {
  qb_sprite_draw_internal(sprite, pos, scale, rot, col);
}

void qb_sprite_drawpart(qbSprite sprite, vec2s pos, int32_t left, int32_t top, int32_t width, int32_t height) {
  qb_sprite_draw_internal(sprite, pos, GLMS_VEC2_ONE_INIT, 0.f, GLMS_VEC4_ONE_INIT, left, top, width, height);
}

void qb_sprite_drawpart_ext(qbSprite sprite, vec2s pos, int32_t left, int32_t top, int32_t width, int32_t height,
                                vec2s scale, float rot, vec4s col) {
  qb_sprite_draw_internal(sprite, pos, scale, rot, col, left, top, width, height);
}

void qb_sprite_setdepth(qbSprite sprite, float depth) {
  sprite->depth = depth;
}

float qb_sprite_getdepth(qbSprite sprite) {
  return sprite->depth;
}

vec2s qb_sprite_getoffset(qbSprite sprite) {
  return sprite->offset;
}

void qb_sprite_setoffset(qbSprite sprite, vec2s offset) {
  sprite->offset = offset;
}

uint32_t qb_sprite_width(qbSprite sprite) {
  if (sprite->animator && sprite->animator->animation) {
    sprite = sprite->animator->animation->frames[sprite->animator->frame];
  }

  return sprite->tw + sprite->margin;
}

uint32_t qb_sprite_height(qbSprite sprite) {
  if (sprite->animator && sprite->animator->animation) {
    sprite = sprite->animator->animation->frames[sprite->animator->frame];
  }

  return sprite->th + sprite->margin;
}

int32_t qb_sprite_framecount(qbSprite sprite) {
  return sprite->animator->animation ? (int32_t)sprite->animator->animation->frames.size() : 1;
}

int32_t qb_sprite_getframe(qbSprite sprite) {
  return sprite->animator->animation ? sprite->animator->frame : 0;
}

void qb_sprite_setframe(qbSprite sprite, int32_t frame_index) {
  if (frame_index == -1) {
    return;
  }

  if (sprite->animator->animation) {
    int32_t framecount = std::max(qb_sprite_framecount(sprite), 1);
    frame_index = std::abs(frame_index);
    frame_index %= framecount;
    sprite->animator->frame = frame_index;
  }
}

qbImage qb_sprite_subimg(qbSprite sprite, int32_t frame) {
  if (!sprite->animator->animation) {
    return sprite->img;
  }

  if (frame == -1) {
    frame = qb_sprite_getframe(sprite);
  }

  int32_t framecount = std::max(qb_sprite_framecount(sprite), 1);
  frame = std::abs(frame);
  frame %= framecount;
  return sprite->animator->animation->frames[frame]->img;
}

qbImage qb_sprite_curimg(qbSprite sprite) {
  qbSpriteAnimator anim = sprite->animator;
  return anim->animation->frames[anim->frame]->img;
}

qbSpriteAnimation qb_spriteanimation_create(qbSpriteAnimationAttr attr) {
  qbSpriteAnimation animation = new qbSpriteAnimation_{};
  uint32_t width = 0;
  uint32_t height = 0;

  for (size_t i = 0; i < attr->frame_count; ++i) {
    if (attr->frames) {
      animation->frames.push_back(attr->frames[i]);
      width = std::max(width, attr->frames[i]->w);
      height = std::max(height, attr->frames[i]->h);
    }
    if (attr->durations) {
      animation->durations.push_back(attr->durations[i] / 1000.0);
    }
  }
  animation->repeat = attr->repeat;
  animation->keyframe = attr->keyframe;
  animation->frame_duration = attr->frame_speed / 1000.0;
  animation->w = width;
  animation->h = height;
  animation->offset = attr->offset;
  return animation;
}

qbSpriteAnimation qb_spriteanimation_fromsheet(qbSpriteAnimationAttr attr, qbSprite sheet, int index_start, int index_end) {
  std::vector<qbSprite> frames;
  std::vector<double> durations;

  int iw = sheet->w / sheet->tw;
  for (int i = index_start; i < index_end; ++i) {
    int ix = i % iw;
    int iy = i / iw;
    frames.push_back(qb_sprite_fromsheet(sheet, ix, iy));
  }
  
  for (int i = 0; i < attr->frame_count; ++i) {
    durations.push_back(attr->durations[i]);
  }

  if (attr->frame_count > frames.size()) {
    for (int i = 0; i < frames.size() - attr->frame_count; ++i) {
      durations.push_back(0.0);
    }
  }

  qbSpriteAnimationAttr_ copy = *attr;
  copy.frames = frames.data();
  copy.durations = durations.data();
  copy.frame_count = frames.size();
  copy.offset = qb_sprite_getoffset(sheet);

  return qb_spriteanimation_create(&copy);
}

qbSprite qb_spriteanimation_play(qbSpriteAnimation animation) {
  qbSprite ret = new qbSprite_{};
  ret->w = animation->w;
  ret->h = animation->h;

  ret->animator = new qbSpriteAnimator_{animation};
  ret->animator->animation = animation;
  ret->animator->frame = animation->keyframe;
  ret->offset = animation->offset;
  return ret;
}

void qb_animator_update(qbSpriteAnimator animator, float dt) {
  qbSpriteAnimation animation = animator->animation;

  if (animation->frames.empty()) {
    return;
  }

  animator->elapsed += dt;
  double frame_time = animation->durations.empty() ? animation->frame_duration : animation->durations[animator->frame];
  if (animator->elapsed >= frame_time) {
    ++animator->frame;
    animator->elapsed -= frame_time;
  }

  if (animator->frame >= animation->frames.size()) {
    if (animation->repeat) {
      animator->frame = animation->keyframe;
    } else {
      --animator->frame;
    }
  }
}

vec2s qb_spriteanimation_getoffset(qbSpriteAnimation animation) {
  return animation->offset;
}

void qb_spriteanimation_setoffset(qbSpriteAnimation animation, vec2s offset) {
  animation->offset = offset;
}

qbRenderPipeline sprite_create_renderpipeline(uint32_t width, uint32_t height) {
  qbRenderPipeline render_pipeline{};

  qbBufferBinding_ binding = {};
  binding.binding = 0;
  binding.stride = SPRITE_VERTEX_ATTRIBUTE_SIZE * sizeof(float);
  binding.input_rate = QB_VERTEX_INPUT_RATE_VERTEX;

  qbVertexAttribute_ attributes[7] = {};
  {
    // Vertex
    qbVertexAttribute_* attr = attributes;
    attr->binding = 0;
    attr->location = 0;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)0;
  }
  {
    // Offset
    qbVertexAttribute_* attr = attributes + 1;
    attr->binding = 0;
    attr->location = 1;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(2 * sizeof(float));
  }
  {
    // Color
    qbVertexAttribute_* attr = attributes + 2;
    attr->binding = 0;
    attr->location = 2;

    attr->count = 4;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(4 * sizeof(float));
  }
  {
    // UVs
    qbVertexAttribute_* attr = attributes + 3;
    attr->binding = 0;
    attr->location = 3;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(8 * sizeof(float));
  }
  {
    // Scale
    qbVertexAttribute_* attr = attributes + 4;
    attr->binding = 0;
    attr->location = 4;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(10 * sizeof(float));
  }
  {
    // Rotation
    qbVertexAttribute_* attr = attributes + 5;
    attr->binding = 0;
    attr->location = 5;

    attr->count = 1;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(12 * sizeof(float));
  }
  {
    // Texture Id
    qbVertexAttribute_* attr = attributes + 6;
    attr->binding = 0;
    attr->location = 6;

    attr->count = 1;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(13 * sizeof(float));
  }

  qbGeometryDescriptor_ geometry_descriptor = {
    .bindings_count = 1,
    .bindings = &binding,

    .attributes_count = sizeof(attributes) / sizeof(attributes[0]),
    .attributes = attributes,

    .mode = QB_DRAW_MODE_TRIANGLES
  };

  qbShaderModule shader_module;
  {
    {
      std::vector<qbShaderResourceBinding_> resources{};
      {
        qbShaderResourceBinding_ info = {};
        info.binding = UniformCamera::Binding();
        info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
        info.stages = QB_SHADER_STAGE_VERTEX;
        info.name = "Camera";

        resources.push_back(info);
      }

      std::vector<std::string> resource_names;
      resource_names.reserve(MAX_BATCH_TEXTURE_UNITS);
      for (uint32_t i = 0; i < MAX_BATCH_TEXTURE_UNITS; ++i) {
        resource_names.push_back(std::string("tex_sampler[") + std::to_string(i) + "]");
        qbShaderResourceBinding_ info;
        info.binding = TEXTURE_UNITS_START_BINDING + i;
        info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
        info.stages = QB_SHADER_STAGE_FRAGMENT;
        info.name = resource_names.back().c_str();

        resources.push_back(info);
      }

      qbShaderResourceLayoutAttr_ attr = {
        .bindings_count = (uint32_t)resources.size(),
        .bindings = resources.data(),
      };
      qb_shaderresourcelayout_create(&sprite_resource_layout, &attr);
    }

    {
      qbShaderResourcePipelineLayoutAttr_ attr = {
        .layouts_count = 1,
        .layouts = &sprite_resource_layout,
      };
      qb_shaderresourcepipelinelayout_create(&sprite_render_pipeline_layout, &attr);
    }
    
    {
      qbShaderModuleAttr_ attr = {};
      attr.vs = get_sprite_vs();
      attr.fs = get_sprite_fs();
      attr.interpret_as_strings = true;

      qb_shadermodule_create(&shader_module, &attr);
    }
  }
  {
    uint8_t white_pixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    qbPixelMap white_pixelmap = qb_pixelmap_create(1, 1, 0, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, white_pixel);

    qbImageAttr_ attr = {};
    attr.type = qbImageType::QB_IMAGE_TYPE_2D;

    qb_image_create(&clear_texture, &attr, white_pixelmap);
  }

  {
    qbColorBlendState_ blend_state{
      .blend_enable = QB_TRUE,
      .rgb_blend = {
        .op = QB_BLEND_EQUATION_ADD,
        .src = QB_BLEND_FACTOR_SRC_ALPHA,
        .dst = QB_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      },
      .alpha_blend = {
        .op = QB_BLEND_EQUATION_ADD,
        .src = QB_BLEND_FACTOR_ONE,
        .dst = QB_BLEND_FACTOR_ZERO,
      },
    };

    qbViewport_ viewport{
      .x = 0,
      .y = 0,
      .w = (float)width,
      .h = (float)height,
      .min_depth = 0.f,
      .max_depth = 1.f
    };

    qbRect_ scissor{
      .x = 0,
      .y = 0,
      .w = (float)width,
      .h = (float)height
    };

    qbViewportState_ viewport_state{
      .viewport = viewport,
      .scissor = scissor
    };

    qbDepthStencilState_ depth_stencil_state{
      .depth_test_enable = QB_TRUE,
      .depth_write_enable = QB_TRUE,
      .depth_compare_op = QB_RENDER_TEST_FUNC_LESS,
      .stencil_test_enable = QB_FALSE,
    };

    qbRasterizationInfo_ raster_info{
      .raster_mode = QB_POLYGON_MODE_FILL,
      .raster_face = QB_FACE_FRONT_AND_BACK,
      .front_face = QB_FRONT_FACE_CCW,
      .cull_face = QB_FACE_NONE,
      .enable_depth_clamp = QB_FALSE,
      .depth_stencil_state = &depth_stencil_state
    };

    qbRenderPipelineAttr_ attr = {
      .shader = shader_module,
      .geometry = &geometry_descriptor,
      .blend_state = &blend_state,
      .viewport_state = &viewport_state,
      .rasterization_info = &raster_info,
      .resource_layout = sprite_render_pipeline_layout
    };

    qb_renderpipeline_create(&render_pipeline, &attr);
  }

  {
    qbFramebufferAttachmentRef_ attachments[2] = {};
    attachments[0] = {
      .attachment = 0,
      .aspect = qbImageAspect::QB_COLOR_ASPECT
    };
    attachments[1] = {
      .attachment = 1,
      .aspect = qbImageAspect::QB_DEPTH_ASPECT
    };

    qbRenderPassAttr_ attr{
      .attachments_count = 2,
      .attachments = attachments,
    };

    qb_renderpass_create(&sprite_render_pass, &attr);
  }

  return render_pipeline;
}

void sprite_initialize(uint32_t width, uint32_t height) {
  sprite_render_pipeline = sprite_create_renderpipeline(width, height);
  sprite_path = std::filesystem::path(qb_resources()->resources) / qb_resources()->images;

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbSprite);
    qb_componentattr_destroy(&attr);
  }
}

void qb_sprite_onresize(uint32_t width, uint32_t height) {
}

typedef struct qbSpriteRenderState_ {
  qbDrawCommandBuffer cmds;
  qbGpuBuffer batch_vbo;
  qbGpuBuffer batch_ibo;
  qbGpuBuffer camera_ubo;
  qbMemoryAllocator allocator;
  std::vector<float> vertex_buffer;
  std::vector<uint32_t> index_buffer;
  qbShaderResourceLayout resource_layout;
  qbShaderResourceSet resource_set;
  qbShaderResourcePipelineLayout shader_pipeline_layout;
} qbSpriteRenderState_, *qbSpriteRenderState;

qbSpriteRenderState qb_spriterenderstate_create(float width, float height) {
  qbGpuBuffer batch_vbo;
  qbGpuBuffer batch_ibo;
  {
    qbGpuBufferAttr_ attr = {
      .size = sizeof(float) * MAX_NUM_SPRITES_PER_BATCH * 4,
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };
    qb_gpubuffer_create(&batch_vbo, &attr);
  }

  {
    qbGpuBufferAttr_ attr = {
      .size = sizeof(uint32_t) * MAX_NUM_SPRITES_PER_BATCH * 6,
      .elem_size = sizeof(uint32_t),
      .buffer_type = QB_GPU_BUFFER_TYPE_INDEX,
    };
    qb_gpubuffer_create(&batch_ibo, &attr);
  }

  qbMemoryAllocator allocator = qb_memallocator_paged(1 << 14);

  qbDrawCommandBuffer cmd_buf;
  {
    qbDrawCommandBufferAttr_ attr = {
      .count = 1,
      .allocator = allocator,
    };
    qb_drawcmd_create(&cmd_buf, &attr);
  }

  qbShaderResourceSet resource_set;
  {
    qbShaderResourceSetAttr_ attr = {
      .create_count = 1,
      .layout = sprite_resource_layout,
    };

    qb_shaderresourceset_create(&resource_set, &attr);
  }

  qbGpuBuffer camera_ubo;
  {
    {
      qbGpuBufferAttr_ attr = {};
      attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
      attr.data = nullptr;
      attr.size = sizeof(UniformCamera);

      qb_gpubuffer_create(&camera_ubo, &attr);
    }

    {
      UniformCamera camera;
      camera.projection = glms_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
      qb_gpubuffer_update(camera_ubo, 0, sizeof(UniformCamera), &camera.projection);
    }

    qb_shaderresourceset_writeuniform(resource_set, UniformCamera::Binding(), camera_ubo);
  }
  {
    for (uint32_t i = 0; i < MAX_BATCH_TEXTURE_UNITS; ++i) {
      qbImageSamplerAttr_ attr = {};
      qbImageSampler sampler;
      attr.image_type = QB_IMAGE_TYPE_2D;
      attr.min_filter = QB_FILTER_TYPE_NEAREST;
      attr.mag_filter = QB_FILTER_TYPE_NEAREST;
      attr.s_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      attr.t_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      qb_imagesampler_create(&sampler, &attr);
      qb_shaderresourceset_writeimage(resource_set, TEXTURE_UNITS_START_BINDING + i, nullptr, sampler);
    }
  }

  qbSpriteRenderState state = new qbSpriteRenderState_{};
  *state = qbSpriteRenderState_{
    .cmds = cmd_buf,
    .batch_vbo = batch_vbo,
    .batch_ibo = batch_ibo,
    .camera_ubo = camera_ubo,
    .allocator = allocator,
    .resource_layout = sprite_resource_layout,
    .resource_set = resource_set,
    .shader_pipeline_layout = sprite_render_pipeline_layout,
  };

  state->vertex_buffer.reserve(MAX_NUM_SPRITES_PER_BATCH * 4);
  state->index_buffer.reserve(MAX_NUM_SPRITES_PER_BATCH * 6);

  return state;
}

qbRenderPipeline qb_sprite_renderpipeline() {
  return sprite_render_pipeline;
}

void qb_spriterenderstate_resize(qbSpriteRenderState renderstate, float width, float height) {
  UniformCamera camera;
  camera.projection = glms_ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
  qb_gpubuffer_update(renderstate->camera_ubo, 0, sizeof(UniformCamera), &camera.projection);
}

void qb_spriterenderstate_record(qbSpriteRenderState state, qbFrameBuffer framebuffer, float width, float height, float dt) {
  qbDrawCommandBuffer draw_cmds = state->cmds;

  if (sprites.empty()) {
    return;
  }

  std::vector<Batch> batches;

  Batch batch{};
  bool has_batch = false;
  while (!sprites.empty()) {
    has_batch = true;
    QueuedSprite s = std::move(sprites.front());
    std::pop_heap(sprites.begin(), sprites.end()); sprites.pop_back();

    // Update the animator here, so that extra work to update them in a
    // separate call isn't done.
    if (s.animator) {
      qb_animator_update(s.animator, dt);
    }

    batch.images.insert(s.sprite_img);
    batch.sprites.push_back(std::move(s));
    if (batch.images.size() >= MAX_BATCH_TEXTURE_UNITS) {
      has_batch = false;
      batches.push_back(std::move(batch));
      batch = {};
    }
  }

  if (has_batch) {
    batches.push_back(std::move(batch));
  }

  qbBeginRenderPassInfo_ begin_info {
    .render_pass = sprite_render_pass,
    .framebuffer = framebuffer,
  };

  qb_drawcmd_beginpass(draw_cmds, &begin_info);
  qb_drawcmd_beginpipeline(draw_cmds, sprite_render_pipeline);

  qbViewport_ viewport{
    .x = 0,
    .y = 0,
    .w = width,
    .h = height,
    .min_depth = 0.f,
    .max_depth = 1.f
  };

  qbRect_ scissor{
    .x = 0,
    .y = 0,
    .w = width,
    .h = height
  };

  qb_drawcmd_setviewport(draw_cmds, &viewport);
  qb_drawcmd_setscissor(draw_cmds, &scissor);

  std::vector<float>& vertices = state->vertex_buffer;
  std::vector<uint32_t>& indices = state->index_buffer;

  for (auto& batch : batches) {
    vertices.resize(0);
    indices.resize(0);

    std::vector<uint32_t> texture_bindings;
    std::vector<qbImage> textures;
    std::unordered_map<qbImage, uint32_t> texture_to_ids;

    for (int i = 0; i < MAX_BATCH_TEXTURE_UNITS; ++i) {
      texture_bindings.push_back(TEXTURE_UNITS_START_BINDING + i);
      textures.push_back(clear_texture);
    }

    int texture_id = 0;
    for (qbImage img : batch.images) {
      textures[texture_id] = img;
      texture_to_ids[img] = texture_id;

      ++texture_id;
    }

    uint32_t index = 0;
    for (auto& queued : batch.sprites) {
      float attributes[SPRITE_VERTEX_ATTRIBUTE_SIZE];
      uint32_t quad_indices[] = {
        4 * index + 3, 4 * index + 1, 4 * index + 0,
        4 * index + 2, 4 * index + 3, 4 * index + 0
      };

      for (uint32_t i = 0; i < 4; ++i) {
        float x = (float)(i & 0x1);
        float y = (float)((i & 0x2) >> 1);
        // Position
        attributes[0] = queued.pos.x;
        attributes[1] = queued.pos.y;

        // Offset
        attributes[2] = queued.w * x - queued.offset_x;
        attributes[3] = queued.h * y - queued.offset_y;

        // Color
        attributes[4] = queued.col.x;
        attributes[5] = queued.col.y;
        attributes[6] = queued.col.z;
        attributes[7] = queued.col.w;

        // Texture ix/iy for atlas.
        attributes[8] = (queued.left + x * (float)queued.w) / (float)queued.sprite_w;
        attributes[9] = (queued.top + y * (float)queued.h) / (float)queued.sprite_h;

        // Scale
        attributes[10] = queued.scale.x;
        attributes[11] = queued.scale.y;

        // Rotation
        attributes[12] = queued.rot;

        // Texture Id
        attributes[13] = (float)texture_to_ids[queued.sprite_img];

        std::copy(attributes, attributes + SPRITE_VERTEX_ATTRIBUTE_SIZE, std::back_inserter(vertices));
      }

      std::copy(quad_indices, quad_indices + (sizeof(quad_indices) / sizeof(quad_indices[0])), std::back_inserter(indices));
      ++index;
    }

    qbShaderResourceSet sprite_resource_set = state->resource_set;
    qb_drawcmd_updateshaderresources(draw_cmds, texture_bindings.size(), texture_bindings.data(), textures.data(), nullptr, sprite_resource_set);
    qb_drawcmd_bindshaderresourceset(draw_cmds, sprite_resource_set);
    qb_drawcmd_pushbuffer(draw_cmds, state->batch_ibo, 0, indices.size() * sizeof(uint32_t), indices.data());
    qb_drawcmd_pushbuffer(draw_cmds, state->batch_vbo, 0, vertices.size() * sizeof(float), vertices.data());
    qb_drawcmd_bindindexbuffer(draw_cmds, state->batch_ibo);
    qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &state->batch_vbo);
    qb_drawcmd_drawindexed(draw_cmds, indices.size(), 0, 0, 0);
  }

  qb_drawcmd_endpass(draw_cmds);
}

qbDrawCommandBuffer qb_spriterenderstate_commands(qbSpriteRenderState state) {
  return state->cmds;
}

qbComponent qb_sprite() {
  return sprite_component;
}