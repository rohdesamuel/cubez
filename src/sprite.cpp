#include <cubez/render.h>
#include <cubez/sprite.h>
#include <cubez/render_pipeline.h>
#include <cglm/struct.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <unordered_map>
#include <vector>


constexpr int MAX_BATCH_TEXTURE_UNITS = 32;
constexpr size_t SPRITE_VERTEX_ATTRIBUTE_SIZE =
  // Position
  3 +

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

struct qbSprite_ {  
  qbImage img;

  // Index of sprite in atlas, -1 if not from atlas.
  int ix, iy;

  // Width and height of the sprite in pixels.
  int w, h;

  // Depth of sprite for painter's algorithm.
  float depth;

  qbSpriteAtlas opt_atlas;
};

struct qbSpriteAtlas_ {
  int w, h;
};

struct QueuedSprite {
  qbSprite sprite;

  // Position to draw at.
  vec2s pos;

  // Scale of sprite.
  vec2s scale;

  // Rotation of sprite in radians.
  float rot;

  // RGBA color.
  vec4s col;

  bool operator<(const QueuedSprite& other) {
    if (sprite->depth == other.sprite->depth) {
      return this < &other;
    }
    return sprite->depth < other.sprite->depth;
  }
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

qbSprite qb_sprite_load(const char* sprite_name, const char* filename) {
  qbImage img{};
  qbImageAttr_ attr{};
  attr.type = qbImageType::QB_IMAGE_TYPE_2D;

  qb_image_load(&img, &attr, filename);

  qbSprite ret = new qbSprite_{};
  ret->w = qb_image_width(img);
  ret->h = qb_image_height(img);
  ret->img = img;

  return ret;
}

qbSprite qb_sprite_find(const char* sprite_name) {
  return nullptr;
}

void qb_sprite_draw(qbSprite sprite, int32_t subimg, vec2s pos) {
  sprites.push_back({ sprite });
  std::push_heap(sprites.begin(), sprites.end());
}

void qb_sprite_draw_ext(qbSprite sprite, int32_t subimg, vec2s pos,
                        vec2s scale, float rot, vec4s col) {
  sprites.push_back({ sprite });
  std::push_heap(sprites.begin(), sprites.end());
}

void qb_sprite_setdepth(qbSprite sprite, float depth) {
  sprite->depth = depth;
}

float qb_sprite_getdepth(qbSprite sprite) {
  return sprite->depth;
}

void qb_sprite_resize(qbSprite sprite) {

}

qbRenderPass sprite_create_renderpass(uint32_t width, uint32_t height) {
  qbRenderPass render_pass;

  qbBufferBinding_ binding = {};
  binding.binding = 0;
  binding.stride = SPRITE_VERTEX_ATTRIBUTE_SIZE * sizeof(float);
  binding.input_rate = QB_VERTEX_INPUT_RATE_VERTEX;

  qbVertexAttribute_ attributes[6] = {};
  {
    // Position
    qbVertexAttribute_* attr = attributes;
    attr->binding = 0;
    attr->location = 0;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)0;
  }
  {
    // Color
    qbVertexAttribute_* attr = attributes + 1;
    attr->binding = 0;
    attr->location = 1;

    attr->count = 4;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(3 * sizeof(float));
  }
  {
    // UVs
    qbVertexAttribute_* attr = attributes + 2;
    attr->binding = 0;
    attr->location = 2;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(7 * sizeof(float));
  }
  {
    // Scale
    qbVertexAttribute_* attr = attributes + 3;
    attr->binding = 0;
    attr->location = 3;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(9 * sizeof(float));
  }
  {
    // Rotation
    qbVertexAttribute_* attr = attributes + 4;
    attr->binding = 0;
    attr->location = 4;

    attr->count = 1;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(11 * sizeof(float));
  }
  {
    // Texture Id
    qbVertexAttribute_* attr = attributes + 5;
    attr->binding = 0;
    attr->location = 5;

    attr->count = 1;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->normalized = false;
    attr->offset = (void*)(12 * sizeof(float));
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

    for (uint32_t i = 0; i < MAX_BATCH_TEXTURE_UNITS; ++i) {
      qbShaderResourceInfo_ info;
      info.binding = 1 + i;
      info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      info.stages = QB_SHADER_STAGE_FRAGMENT;
      info.name = "tex_sampler";

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
    qbImageSampler image_samplers[1];
    {
      qbImageSamplerAttr_ attr = {};
      attr.image_type = QB_IMAGE_TYPE_2D;
      attr.min_filter = QB_FILTER_TYPE_LINEAR;
      attr.mag_filter = QB_FILTER_TYPE_LINEAR;
      attr.s_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      attr.t_wrap = QB_IMAGE_WRAP_TYPE_REPEAT;
      qb_imagesampler_create(image_samplers, &attr);
    }

    uint32_t bindings[] = { 1 };
    qbImageSampler* samplers = image_samplers;
    qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
  }

  {
    qbRenderPassAttr_ attr = {};
    attr.name = "Sprite Render Pass";
    attr.supported_geometry.bindings = &binding;
    attr.supported_geometry.bindings_count = 1;
    attr.supported_geometry.attributes = attributes;
    attr.supported_geometry.attributes_count = 3;
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

  return render_pass;
}

void qb_sprite_initialize(uint32_t width, uint32_t height) {
  sprite_render_pass = sprite_create_renderpass(width, height);
}

void qb_sprite_onresize(uint32_t width, uint32_t height) {
  {
    UniformCamera camera;
    camera.projection = glms_ortho(0.0f, (float)width, (float)height, 0.0f, -2.0f, 2.0f);
    qb_gpubuffer_update(camera_ubo, 0, sizeof(UniformCamera), &camera.projection);
  }

  qb_renderpass_resize(sprite_render_pass, { 0.0, 0.0, (float)width, (float)height });
}

void qb_sprite_flush(qbFrameBuffer frame) {
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
    std::vector<uint32_t> texture_ids;
    std::vector<qbImage> textures;
    std::unordered_map<qbImage, uint32_t> texture_to_ids;

    int texture_id = 1;
    for (qbImage img : batch.images) {
      texture_ids.push_back(texture_id);
      textures.push_back(img);
      texture_to_ids[img] = texture_id;

      ++texture_id;
    }

    uint32_t index = 0;
    for (auto& queued : batch.sprites) {
      float attributes[SPRITE_VERTEX_ATTRIBUTE_SIZE];
      uint32_t quad_indices[] = {
        6 * index + 3, 6 * index + 1, 6 * index + 0,
        6 * index + 2, 6 * index + 3, 6 * index + 0
      };

      for (uint32_t i = 0; i < 4; ++i) {
        float x = i & 0x1;
        float y = (i & 0x2) >> 1;
        // Position
        attributes[0] = queued.pos.x + queued.sprite->w * x;
        attributes[1] = queued.pos.y + queued.sprite->h * y;
        attributes[2] = 0.f;

        // Color
        attributes[3] = queued.col.x;
        attributes[4] = queued.col.y;
        attributes[5] = queued.col.z;
        attributes[6] = queued.col.w;

        // Texture ix/iy for atlas.
        if (queued.sprite->opt_atlas) {
          attributes[7] = ((queued.sprite->ix + x) * queued.sprite->w) / queued.sprite->opt_atlas->w;
          attributes[8] = ((queued.sprite->iy + y) * queued.sprite->h) / queued.sprite->opt_atlas->h;
        } else {
          attributes[7] = x;
          attributes[8] = y;
        }

        // Scale
        attributes[9] = queued.scale.x;
        attributes[10] = queued.scale.y;

        // Rotation
        attributes[11] = queued.rot;

        // Texture Id
        attributes[12] = texture_to_ids[queued.sprite->img];

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

    qb_meshbuffer_updateimages(sprite_quads, textures.size(), texture_ids.data(), textures.data());
    
    qb_renderpass_drawto(sprite_render_pass, frame, 1, &batched_sprites);
  }
}