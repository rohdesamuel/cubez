#include <cubez/render.h>
#include <cubez/sprite.h>
#include <cubez/render_pipeline.h>
#include <cglm/struct.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>


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

struct qbAnimator_ {
  qbAnimation animation;
  int frame;
  double elapsed;
};

struct qbSprite_ {  
  qbImage img;

  vec2s offset;

  // Index of sprite in atlas, -1 if not from atlas.
  int ix, iy;

  // Width and height of the sprite in pixels.
  uint32_t w, h;

  // Width and height of the tile.
  uint32_t tw, th;

  uint32_t margin;

  // Depth of sprite for painter's algorithm.
  float depth;

  // If the sprite is animated, this will point to the animation.  
  qbAnimator_ animator;
};

struct QueuedSprite {
  qbSprite sprite;
  float depth;

  // Position to draw at.
  vec2s pos;

  // Scale of sprite.
  vec2s scale;

  // Rotation of sprite in radians.
  float rot;

  // RGBA color.
  vec4s col;

  // If the sprite is part of an animation, then this will point to its
  // animator.
  qbAnimator animator;

  bool operator<(const QueuedSprite& other) {
    if (depth == depth) {
      return this < &other;
    }
    return depth < other.depth;
  }
};

struct qbAnimation_ {
  std::vector<qbSprite> frames;
  std::vector<double> durations;

  double frame_duration;
  bool repeat;
  int keyframe;

  uint32_t w, h;
};

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

qbSprite qb_spritesheet_load(const char* sheet_name, const char* filename, int tw, int th, int margin) {
  qbSprite sheet = qb_sprite_load(sheet_name, filename);
  sheet->tw = tw;
  sheet->th = th;
  sheet->margin = margin;

  return sheet;
}

qbSprite qb_sprite_fromsheet(const char* sprite_name, qbSprite sheet, int ix, int iy) {
  qbSprite ret = new qbSprite_{};
  ret->w = sheet->w;
  ret->h = sheet->h;
  ret->img = sheet->img;
  ret->margin = sheet->margin;
  ret->tw = sheet->tw;
  ret->th = sheet->th;
  ret->ix = ix;
  ret->iy = iy;

  return ret;
}

qbSprite qb_sprite_load(const char* sprite_name, const char* filename) {
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

qbSprite qb_sprite_find(const char* sprite_name) {
  return nullptr;
}

void qb_sprite_draw_internal(qbSprite sprite, vec2s pos, vec2s scale, float rot, vec4s col) {
  if (sprite->animator.animation) {
    qbSprite frame = sprite->animator.animation->frames[sprite->animator.frame];
    sprites.push_back({ frame, sprite->depth, pos, scale, rot, col, &sprite->animator });
  } else {
    sprites.push_back({ sprite, sprite->depth, pos, scale, rot, col });
  }
  std::push_heap(sprites.begin(), sprites.end());
}

void qb_sprite_draw(qbSprite sprite, vec2s pos) {
  qb_sprite_draw_internal(sprite, pos, GLMS_VEC2_ONE_INIT, 0.f, GLMS_VEC4_ONE_INIT);
}

void qb_sprite_draw_ext(qbSprite sprite, vec2s pos, vec2s scale, float rot, vec4s col) {
  qb_sprite_draw_internal(sprite, pos, scale, rot, col);
}

void qb_sprite_setdepth(qbSprite sprite, float depth) {
  sprite->depth = depth;
}

float qb_sprite_getdepth(qbSprite sprite) {
  return sprite->depth;
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

qbAnimation qb_animation_create(const char* animation_name, qbAnimationAttr attr) {
  qbAnimation animation = new qbAnimation_{};
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
  return animation;
}

qbAnimation qb_animation_fromsheet(const char* animation_name, qbAnimationAttr attr, qbSprite sheet,
                                   int index_start, int index_end) {
  std::vector<qbSprite> frames;
  std::vector<double> durations;

  int iw = sheet->w / sheet->tw;
  for (int i = index_start; i < index_end; ++i) {
    int ix = i % iw;
    int iy = i / iw;
    frames.push_back(qb_sprite_fromsheet(nullptr, sheet, ix, iy));
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

  return qb_animation_create(animation_name, attr);
}

qbSprite qb_animation_play(qbAnimation animation) {
  qbSprite ret = new qbSprite_{};
  ret->w = animation->w;
  ret->h = animation->h;

  ret->animator.animation = animation;
  ret->animator.frame = animation->keyframe;
  return ret;
}

void qb_animator_update(qbAnimator animator, qbRenderEvent e) {
  qbAnimation animation = animator->animation;

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

qbRenderPass sprite_create_renderpass(uint32_t width, uint32_t height) {
  qbRenderPass render_pass;

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
    std::vector<qbShaderResourceInfo_> resources{};
    {      
      qbShaderResourceInfo_ info;
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
      qbShaderResourceInfo_ info;
      info.binding = 1 + i;
      info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      info.stages = QB_SHADER_STAGE_FRAGMENT;
      info.name = resource_names.back().c_str();

      resources.push_back(info);
    }

    qbShaderModuleAttr_ attr = {};
    attr.vs = "resources/sprite.vs";
    attr.fs = "resources/sprite.fs";

    attr.resources = resources.data();
    attr.resources_count = resources.size();

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
    qb_shadermodule_attachuniforms(shader_module, 1, bindings, ubo_buffers);

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

    qb_shadermodule_attachsamplers(shader_module, bindings.size(), bindings.data(), image_samplers.data());
  }

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
    attr.cull = QB_CULL_BACK;

    qbClearValue_ clear;
    clear.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
    clear.color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clear.depth = 1.0f;
    attr.clear = clear;

    qb_renderpass_create(&render_pass, &attr);
  }

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
    qbRenderGroupAttr_ attr = {};
    qb_rendergroup_create(&batched_sprites, &attr);
  }
  {
    qbMeshBufferAttr_ attr{};
    attr.descriptor = *qb_renderpass_geometry(render_pass);

    qb_meshbuffer_create(&sprite_quads, &attr);

    qbGpuBuffer vertex_buffers[] = { batch_vbo };
    qb_meshbuffer_attachvertices(sprite_quads, vertex_buffers);
    qb_meshbuffer_attachindices(sprite_quads, batch_ebo);

    qb_rendergroup_append(batched_sprites, sprite_quads);
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

void qb_sprite_initialize(uint32_t width, uint32_t height) {
  sprite_render_pass = sprite_create_renderpass(width, height);

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbSprite);
    qb_componentattr_destroy(&attr);
  }
}

void qb_sprite_onresize(uint32_t width, uint32_t height) {
  {
    UniformCamera camera;
    camera.projection = glms_ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
    qb_gpubuffer_update(camera_ubo, 0, sizeof(UniformCamera), &camera.projection);
  }

  qb_renderpass_resize(sprite_render_pass, { 0.0, 0.0, (float)width, (float)height });
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

      for (uint32_t i = 0; i < 4; ++i) {
        float x = i & 0x1;
        float y = (i & 0x2) >> 1;
        // Position
        attributes[0] = queued.sprite->tw * x - queued.sprite->offset.x;
        attributes[1] = queued.sprite->th * y - queued.sprite->offset.y;

        // Offset
        attributes[2] = queued.pos.x;
        attributes[3] = queued.pos.y;

        // Color
        attributes[4] = queued.col.x;
        attributes[5] = queued.col.y;
        attributes[6] = queued.col.z;
        attributes[7] = queued.col.w;

        // Texture ix/iy for atlas.
        attributes[8] = (queued.sprite->ix + x) * ((float)(queued.sprite->tw + queued.sprite->margin) / (float)queued.sprite->w);
        attributes[9] = (queued.sprite->iy + y) * ((float)(queued.sprite->th + queued.sprite->margin) / (float)queued.sprite->h);

        // Scale
        attributes[10] = queued.scale.x;
        attributes[11] = queued.scale.y;

        // Rotation
        attributes[12] = queued.rot;

        // Texture Id
        attributes[13] = texture_to_ids[queued.sprite->img];

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

    qb_meshbuffer_updateimages(sprite_quads, textures.size(), texture_bindings.data(), textures.data());
    
    qb_renderpass_drawto(sprite_render_pass, frame, 1, &batched_sprites);
  }
}

qbComponent qb_sprite() {
  return sprite_component;
}