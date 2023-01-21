#include <cubez/render.h>
#include <cubez/sprite.h>
#include <cubez/render_pipeline.h>
#include <cglm/struct.h>

#include <algorithm>
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
  1 +
  
  // Entity Id
  4;
constexpr size_t MAX_NUM_SPRITES_PER_BATCH = 1000;

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
  qbSprite sprite;
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
  qbSprite animator_sprite;
  qbSpriteAnimator animator;

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

qbGpuBuffer batch_vbo;
qbGpuBuffer batch_ebo;

static qbGpuBuffer camera_ubo;
qbRenderPass sprite_render_pass;

qbRenderGroup batched_sprites;
qbMeshBuffer sprite_quads;

// A 1x1 white texture that is used to render solid color sprites.
qbImage clear_texture;

qbComponent sprite_component;

qbSprite qb_spritesheet_load(const char* filename, int tw, int th, int margin) {
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

qbSprite qb_sprite_load(const char* filename) {
  std::filesystem::path path = std::filesystem::path(qb_resources()->dir) / std::filesystem::path(qb_resources()->sprites) / filename;

  qbImage img{};
  qbImageAttr_ attr{};
  attr.type = qbImageType::QB_IMAGE_TYPE_2D;

  qb_image_load(&img, &attr, path.string().c_str());

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

qbSpriteAnimation qb_spriteanimation_loaddir(const char* dir, qbSpriteAnimationAttr attr) {
  thread_local static char buf[512];

  const std::filesystem::path path = std::filesystem::path(qb_resources()->dir) / std::filesystem::path(qb_resources()->sprites) / dir;

  std::vector<qbSprite> frames;
  for (const auto& entry : std::filesystem::directory_iterator(path)) {    
    size_t written = wcstombs(buf, (std::filesystem::path(dir) / entry.path().filename()).c_str(), sizeof(buf));
    if (written == sizeof(buf)) {
      buf[sizeof(buf) - 1] = '\0';
    }

    frames.push_back(qb_sprite_load(buf));
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

  sprites.push_back({ sprite, sprite->depth, sprites.size(), pos, scale, rot, col, left, top, width, height, animator_sprite, animator });
  std::push_heap(sprites.begin(), sprites.end());
}

void qb_sprite_draw_internal(qbSprite sprite, vec2s pos, vec2s scale, float rot, vec4s col) {
  qbSprite animator_sprite = nullptr;
  qbSpriteAnimator animator = nullptr;

  if (sprite->animator->animation) {
    animator_sprite = sprite;
    animator = sprite->animator;
    sprite = sprite->animator->animation->frames[sprite->animator->frame];
  }

  int32_t width = sprite->tw + sprite->margin;
  int32_t height = sprite->th + sprite->margin;
  int32_t left = sprite->ix * width;
  int32_t top = sprite->iy * height;

  sprites.push_back({ sprite, sprite->depth, sprites.size(), pos, scale, rot, col, left, top, width, height, animator_sprite, animator });
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
  return sprite->w;
}

uint32_t qb_sprite_height(qbSprite sprite) {
  return sprite->h;
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

  attr->frames = frames.data();
  attr->durations = durations.data();
  attr->frame_count = frames.size();
  attr->offset = qb_sprite_getoffset(sheet);

  return qb_spriteanimation_create(attr);
}

qbSprite qb_spriteanimation_play(qbSpriteAnimation animation) {
  qbSprite ret = new qbSprite_{};
  ret->w = animation->w;
  ret->h = animation->h;

  ret->animator->animation = animation;
  ret->animator->frame = animation->keyframe;
  ret->offset = animation->offset;
  return ret;
}

void qb_animator_update(qbSpriteAnimator animator, qbRenderEvent e) {
  qbSpriteAnimation animation = animator->animation;

  if (animation->frames.empty()) {
    return;
  }

  animator->elapsed += e->dt;
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

qbRenderPass sprite_create_renderpass(uint32_t width, uint32_t height) {
  qbRenderPass render_pass{};

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

  qbShaderModule shader_module;
  {
    std::vector<qbShaderResourceBinding_> resources{};
    {      
      qbShaderResourceBinding_ info;
      info.binding = UniformCamera::Binding();
      info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
      info.stages = QB_SHADER_STAGE_VERTEX;
      info.name = "Camera";

      resources.push_back(info);
    }

    std::vector<std::string> resource_names;
    resource_names.reserve(MAX_BATCH_TEXTURE_UNITS + 1);
    for (uint32_t i = 0; i < MAX_BATCH_TEXTURE_UNITS + 1; ++i) {
      resource_names.push_back(std::string("tex_sampler[") + std::to_string(i) + "]");
      qbShaderResourceBinding_ info;
      info.binding = 1 + i;
      info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      info.stages = QB_SHADER_STAGE_FRAGMENT;
      info.name = resource_names.back().c_str();

      resources.push_back(info);
    }
    
    qbShaderModuleAttr_ attr = {};
    attr.vs = get_sprite_vs();
    attr.fs = get_sprite_fs();
    attr.interpret_as_strings = true;

    //attr.resources = resources.data();
    //attr.resources_count = resources.size();

    qb_shadermodule_create(&shader_module, &attr);
  }
  {
    {
      qbGpuBufferAttr_ attr = {};
      attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
      attr.data = nullptr;
      attr.size = sizeof(UniformCamera);

      qb_gpubuffer_create(&camera_ubo, &attr);
    }

    uint32_t bindings[] = { UniformCamera::Binding() };
    qbGpuBuffer ubo_buffers[] = { camera_ubo };
    //qb_shadermodule_attachuniforms(shader_module, 1, bindings, ubo_buffers);

    {
      UniformCamera camera;
      camera.projection = glms_ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
      qb_gpubuffer_update(camera_ubo, 0, sizeof(UniformCamera), &camera.projection);
    }
  }
  {
    std::vector<uint32_t> bindings;
    std::vector<qbImageSampler> image_samplers;
    for (uint32_t i = 0; i < MAX_BATCH_TEXTURE_UNITS + 1; ++i) {
      bindings.push_back(1 + i);

      qbImageSamplerAttr_ attr = {};
      qbImageSampler sampler;
      attr.image_type = QB_IMAGE_TYPE_2D;
      attr.min_filter = QB_FILTER_TYPE_NEAREST;
      attr.mag_filter = QB_FILTER_TYPE_NEAREST;
      attr.s_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      attr.t_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      qb_imagesampler_create(&sampler, &attr);

      image_samplers.push_back(sampler);
    }

    //qb_shadermodule_attachsamplers(shader_module, bindings.size(), bindings.data(), image_samplers.data());
  }
  /*
  {
    qbRenderPassAttr_ attr = {};
    attr.name = "Sprite Render Pass";
    attr.supported_geometry.bindings = &binding;
    attr.supported_geometry.bindings_count = 1;
    attr.supported_geometry.attributes = attributes;
    attr.supported_geometry.attributes_count = sizeof(attributes) / sizeof(attributes[0]);
    attr.supported_geometry.mode = QB_DRAW_MODE_TRIANGLES;
    attr.shader = shader_module;
    attr.viewport = { 0.0, 0.0, (float)width, (float)height };
    attr.viewport_scale = 1.0f;
    attr.cull = QB_FACE_NONE;

    qbClearValue_ clear{};
    clear.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
    clear.color = { 1.0f, 0.0f, 1.0f, 1.0f };
    clear.depth = 1.0f;
    attr.clear = clear;

    qb_renderpass_create(&render_pass, &attr);
  }
  */
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
    attr.elem_size = sizeof(float);

    qb_gpubuffer_create(&batch_vbo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.elem_size = sizeof(int);

    qb_gpubuffer_create(&batch_ebo, &attr);
  }
  {
    //qbRenderGroupAttr_ attr = {};
    //qb_rendergroup_create(&batched_sprites, &attr);
  }
  {
    qbMeshBufferAttr_ attr{};
    //attr.descriptor = *qb_renderpass_geometry(render_pass);

    qb_meshbuffer_create(&sprite_quads, &attr);

    qbGpuBuffer vertex_buffers[] = { batch_vbo };
    qb_meshbuffer_attachvertices(sprite_quads, vertex_buffers, 0);
    qb_meshbuffer_attachindices(sprite_quads, batch_ebo, 0);

    //qb_rendergroup_append(batched_sprites, sprite_quads);
  }
  {
    uint8_t white_pixel[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    qbPixelMap white_pixelmap = qb_pixelmap_create(1, 1, 0, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, white_pixel);

    qbImageAttr_ attr = {};
    attr.type = qbImageType::QB_IMAGE_TYPE_2D;

    qb_image_create(&clear_texture, &attr, white_pixelmap);
  }

  return render_pass;
}

void sprite_initialize(uint32_t width, uint32_t height) {
  return;
  sprite_render_pass = sprite_create_renderpass(width, height);
  sprite_path = std::filesystem::path(qb_resources()->dir) / qb_resources()->sprites;

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbSprite);
    qb_componentattr_destroy(&attr);
  }
}

void qb_sprite_onresize(uint32_t width, uint32_t height) {
  {
    UniformCamera camera{};
    camera.projection = glms_ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
    qb_gpubuffer_update(camera_ubo, 0, sizeof(UniformCamera), &camera.projection);
  }
}

void qb_sprite_flush(qbFrameBuffer frame, qbRenderEvent e) {
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
      qb_animator_update(s.animator, e);
    }

    batch.images.insert(s.sprite->img);
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

  for (auto& batch : batches) {
    std::vector<float> vertices;
    std::vector<int> indices;
    std::vector<uint32_t> texture_bindings;
    std::vector<qbImage> textures;
    std::unordered_map<qbImage, uint32_t> texture_to_ids;

    for (int i = 0; i < MAX_BATCH_TEXTURE_UNITS + 1; ++i) {
      texture_bindings.push_back(i + 1);
      textures.push_back(clear_texture);
    }
    texture_to_ids[clear_texture] = 0;

    int texture_id = 1;
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

      float offset_x = queued.animator ? queued.animator_sprite->offset.x : queued.sprite->offset.x;
      float offset_y = queued.animator ? queued.animator_sprite->offset.y : queued.sprite->offset.y;

      for (uint32_t i = 0; i < 4; ++i) {
        float x = (float)(i & 0x1);
        float y = (float)((i & 0x2) >> 1);
        // Position
        attributes[0] = queued.w * x - offset_x;
        attributes[1] = queued.h * y - offset_y;

        // Offset
        attributes[2] = queued.pos.x;
        attributes[3] = queued.pos.y;

        // Color
        attributes[4] = queued.col.x;
        attributes[5] = queued.col.y;
        attributes[6] = queued.col.z;
        attributes[7] = queued.col.w;

        // Texture ix/iy for atlas.
        attributes[8] = (queued.left + x * (float)queued.w) / (float)queued.sprite->w;
        attributes[9] = (queued.top + y * (float)queued.h) / (float)queued.sprite->h;

        // Scale
        attributes[10] = queued.scale.x;
        attributes[11] = queued.scale.y;

        // Rotation
        attributes[12] = queued.rot;

        // Texture Id
        attributes[13] = (float)texture_to_ids[queued.sprite->img];

        std::copy(attributes, attributes + SPRITE_VERTEX_ATTRIBUTE_SIZE, std::back_inserter(vertices));        
      }

      std::copy(quad_indices, quad_indices + (sizeof(quad_indices) / sizeof(quad_indices[0])), std::back_inserter(indices));
      ++index;
    }

    qbGpuBuffer* vertex_buffers;
    qb_meshbuffer_vertices(sprite_quads, &vertex_buffers);
    qb_gpubuffer_resize(vertex_buffers[0], vertices.size() * sizeof(float));
    qb_gpubuffer_update(vertex_buffers[0], 0, vertices.size() * sizeof(float), vertices.data());

    qbGpuBuffer index_buffer;
    qb_meshbuffer_indices(sprite_quads, &index_buffer);
    qb_gpubuffer_resize(index_buffer, indices.size() * sizeof(uint32_t));
    qb_gpubuffer_update(index_buffer, 0, indices.size() * sizeof(uint32_t), indices.data());
    qb_meshbuffer_setcount(sprite_quads, indices.size());

    qb_meshbuffer_updateimages(sprite_quads, textures.size(), texture_bindings.data(), textures.data());  
    
    //qb_renderpass_drawto(sprite_render_pass, frame, 1, &batched_sprites);
  }
}

qbComponent qb_sprite() {
  return sprite_component;
}