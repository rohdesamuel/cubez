#include <cubez/renderer.h>

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cubez/mesh.h>
#include <cubez/render.h>
#include <cubez/draw.h>
#include <cubez/memory.h>

#include <cglm/struct.h>
#include <array>
#include <unordered_map>

const size_t MAX_TRI_BATCH_COUNT = 100;

struct CameraUbo {
  mat4s viewproj;
  float frame;
};

struct ModelUbo {
  mat4s model;
};

struct MaterialUbo {
  float albedo[3];
  float metallic;
  float emission[3];
  float roughness;
};

struct Vertex {
  float pos[3];
  float norm[3];
  float tex[2];
};

struct LightPoint {
  // Interleave the brightness and radius to optimize std140 wasted bytes.
  vec3s rgb;
  float brightness;
  vec3s pos;
  float radius;
};

struct LightDirectional {
  vec3s rgb;
  float brightness;
  vec3s dir;
  float _dir_pad;
};

struct LightSpot {
  vec3s rgb;
  float brightness;
  vec3s pos;
  float range;
  vec3s dir;
  float angle_deg;
};

struct DrawCommandQueue {
  void push(qbDrawCommand_&& command) {
    commands[command.args.material].push_back(std::move(command));
  }

  std::unordered_map<qbMaterial, std::vector<qbDrawCommand_>> commands;
};

const size_t kMaxDirectionalLights = 4;
const size_t kMaxPointLights = 32;
const size_t kMaxSpotLights = 8;

struct qbDefaultRenderer_ {
  qbRenderer_ renderer;

  qbGeometryDescriptor geometry;
  qbShaderResourceLayout resource_layout;
  qbShaderResourcePipelineLayout pipeline_layout;
  qbShaderModule shader_module;
  qbSwapchain swapchain;
  qbImage depth_image;

  qbRenderPass render_pass;
  qbRenderPipeline render_pipeline;
  qbMemoryAllocator cmd_buf_allocator;
  qbMemoryAllocator draw_allocator;
  qbMemoryAllocator allocator;

  qbGpuBuffer quad_buffer;
  qbGpuBuffer camera_ubo;
  qbGpuBuffer material_ubo;
  qbGpuBuffer model_ubo;  

  std::array<struct LightDirectional, kMaxDirectionalLights> directional_lights;
  std::array<bool, kMaxDirectionalLights> enabled_directional_lights;
  std::array<struct LightPoint, kMaxPointLights> point_lights;
  std::array<bool, kMaxPointLights> enabled_point_lights;
  std::array<struct LightSpot, kMaxSpotLights> spot_lights;
  std::array<bool, kMaxSpotLights> enabled_spot_lights;

  // Render queue.
  std::vector<qbTask> render_tasks;
  std::vector<qbClearValue_> clear_values;
  std::vector<qbDrawCommandBuffer> cmd_bufs;
  std::vector<qbShaderResourceSet> resource_sets;
  std::vector<qbFrameBuffer> fbos;
  std::vector<qbImage> swapchain_images;  
  std::vector<CameraUbo> camera_data;

  DrawCommandQueue draw_commands;
};

qbGpuBuffer tri_verts;
qbGpuBuffer quad_verts;
qbGpuBuffer circle_verts;
size_t circle_verts_count;

qbGpuBuffer box_verts;
size_t box_verts_count;

qbGeometryDescriptor create_geometry_descriptor() {
  qbGeometryDescriptor geometry = new qbGeometryDescriptor_;
  {
    qbBufferBinding bindings = new qbBufferBinding_[1];
    bindings[0] = qbBufferBinding_{
      .binding = 0,
      .stride = sizeof(Vertex),
    };

    qbVertexAttribute attributes = new qbVertexAttribute_[3];
    attributes[0] = qbVertexAttribute_{
      .binding = 0,
      .location = 0,
      .count = 3,
      .type = QB_VERTEX_ATTRIB_TYPE_FLOAT,
      .name = QB_VERTEX_ATTRIB_NAME_POSITION,
      .offset = (void*)offsetof(Vertex, pos)
    };

    attributes[1] = qbVertexAttribute_{
      .binding = 0,
      .location = 1,
      .count = 3,
      .type = QB_VERTEX_ATTRIB_TYPE_FLOAT,
      .name = QB_VERTEX_ATTRIB_NAME_NORMAL,
      .offset = (void*)offsetof(Vertex, norm)
    };

    attributes[2] = qbVertexAttribute_{
      .binding = 0,
      .location = 2,
      .count = 2,
      .type = QB_VERTEX_ATTRIB_TYPE_FLOAT,
      .name = QB_VERTEX_ATTRIB_NAME_TEXTURE,
      .offset = (void*)offsetof(Vertex, tex)
    };

    geometry->attributes = attributes;
    geometry->attributes_count = 3;
    geometry->bindings = bindings;
    geometry->bindings_count = 1;
    geometry->mode = QB_DRAW_MODE_TRIANGLES;
  }

  return geometry;
}

qbGpuBuffer create_camera_uniform() {
  CameraUbo camera_data{};
  qbGpuBufferAttr_ attr{
    .data = &camera_data,
    .size = sizeof(CameraUbo),
    .buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM,
  };

  qbGpuBuffer ret;
  qb_gpubuffer_create(&ret, &attr);
  return ret;
}

qbGpuBuffer create_model_uniform() {
  ModelUbo camera_data{};
  qbGpuBufferAttr_ attr{
    .data = &camera_data,
    .size = sizeof(ModelUbo),
    .buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM,
  };

  qbGpuBuffer ret;
  qb_gpubuffer_create(&ret, &attr);
  return ret;
}

qbGpuBuffer create_material_uniform() {
  MaterialUbo material_data{};
  qbGpuBufferAttr_ attr{
    .data = &material_data,
    .size = sizeof(MaterialUbo),
    .buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM,
  };

  qbGpuBuffer ret;
  qb_gpubuffer_create(&ret, &attr);
  return ret;
}

qbShaderResourceLayout create_resource_layout() {
  qbShaderResourceLayout resource_layout;
  qbShaderResourceBinding_ bindings[] = {
    {
      .name = "camera_ubo",
      .binding = 0,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_VERTEX,
    },
    {
      .name = "material_ubo",
      .binding = 1,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_FRAGMENT,
    },
    {
      .name = "model_ubo",
      .binding = 2,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_VERTEX,
    },
    /*{
      .name = "texSampler",
      .binding = 1,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },*/
  };

  qbShaderResourceLayoutAttr_ attr{
    .binding_count = sizeof(bindings) / sizeof(qbShaderResourceBinding_),
    .bindings = bindings,
  };

  qb_shaderresourcelayout_create(&resource_layout, &attr);

  return resource_layout;
}

qbShaderResourcePipelineLayout create_resource_pipeline_layout(qbShaderResourceLayout resource_layout) {
  qbShaderResourcePipelineLayout resource_pipeline_layout;

  qbShaderResourcePipelineLayoutAttr_ attr{
    .layout_count = 1,
    .layouts = &resource_layout
  };

  qb_shaderresourcepipelinelayout_create(&resource_pipeline_layout, &attr);
  return resource_pipeline_layout;
}

qbShaderModule create_shader_module() {
  qbShaderModule shader;
  qbShaderModuleAttr_ attr{};
  attr.vs = R"(      
    #version 330 core

    layout (location = 0) in vec3 in_pos;
    layout (location = 1) in vec3 in_norm;
    layout (location = 2) in vec2 in_tex;

    layout(std140, binding = 0) uniform CameraUbo {
        mat4 viewproj;
        float frame;
    } camera_ubo;

    layout(std140, binding = 2) uniform ModelUbo {
        mat4 model;
    } model_ubo;

    out VertexData
    {
      vec3 norm;
      vec2 tex;
    } o;

    void main() {        
      o.norm = in_norm;
      o.tex = in_tex;

      gl_Position = camera_ubo.viewproj * model_ubo.model * vec4(in_pos, 1.0);
    })";

  attr.fs = R"(
    #version 330 core

    //uniform sampler2D texSampler;

    layout (location = 0) out vec4 out_color;

    layout (std140, binding = 1) uniform MaterialUbo {
      vec3 albedo;
      float metallic;
      vec3 emission;
      float roughness;
    } material_ubo;

    in VertexData
    {
	    vec3 norm;
      vec2 tex;
    } o;

    void main() {
      out_color = vec4(material_ubo.albedo, 1.f);// * vec4(texture(texSampler, o.tex).rgb, 1.f);
    })";

  attr.interpret_as_strings = QB_TRUE;

  qb_shadermodule_create(&shader, &attr);

  return shader;
}

void create_swapchain(uint32_t width, uint32_t height, qbRenderPass render_pass, qbDefaultRenderer r) {
  {
    qbSwapchainAttr_ attr{
      .extent = {
        .w = (float)width,
        .h = (float)height
      }
    };
    qb_swapchain_create(&r->swapchain, &attr);
  }

  size_t swapchain_image_count;
  qb_swapchain_images(r->swapchain, &swapchain_image_count, nullptr);  
  r->swapchain_images = std::vector<qbImage>(swapchain_image_count, {});
  qb_swapchain_images(r->swapchain, &swapchain_image_count, r->swapchain_images.data());

  {
    qbImageAttr_ attr{
      .type = QB_IMAGE_TYPE_2D
    };
    qb_image_raw(
      &r->depth_image, &attr, QB_PIXEL_FORMAT_D24_S8,
      qb_window_width(), qb_window_height(), nullptr);
  }

  r->fbos = std::vector<qbFrameBuffer>(swapchain_image_count, nullptr);
  for (size_t i = 0; i < swapchain_image_count; ++i) {
    qbFramebufferAttachment_ attachments[] = {
      { r->swapchain_images[i], QB_COLOR_ASPECT },
      { r->depth_image, QB_DEPTH_STENCIL_ASPECT },
    };

    qbFrameBufferAttr_ attr{
      .render_pass = render_pass,
      .attachments = attachments,
      .attachments_count = 2,
      .width = width,
      .height = height,
    };

    qb_framebuffer_create(&r->fbos[i], &attr);
  }

  r->camera_data = std::vector<CameraUbo>(swapchain_image_count, CameraUbo{});
}

qbRenderPass create_renderpass() {
  qbRenderPass render_pass;
  qbFramebufferAttachmentRef_ attachments[2] = {};
  attachments[0] = {
    .attachment = 0,
    .aspect = qbImageAspect::QB_COLOR_ASPECT
  };
  attachments[1] = {
    .attachment = 1,
    .aspect = qbImageAspect::QB_DEPTH_STENCIL_ASPECT
  };

  qbRenderPassAttr_ attr{
    .attachments = attachments,
    .attachments_count = 2
  };

  qb_renderpass_create(&render_pass, &attr);

  return render_pass;
}

qbRenderPipeline create_render_pipeline(
  qbShaderModule shader,
  qbGeometryDescriptor geometry,
  qbRenderPass render_pass,
  qbShaderResourcePipelineLayout resource_pipeline_layout) {

  qbRenderPipeline pipeline;
  {
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
      .cull_face = QB_FACE_BACK,
      .enable_depth_clamp = QB_FALSE,
      .depth_stencil_state = &depth_stencil_state
    };

    qbColorBlendState_ blend_state{
      .blend_enable = QB_FALSE,
      .rgb_blend = {
        .op = QB_BLEND_EQUATION_ADD,
        .src = QB_BLEND_FACTOR_SRC_ALPHA,
        .dst = QB_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      },
      .alpha_blend = {
        .op = QB_BLEND_EQUATION_ADD,
        .src = QB_BLEND_FACTOR_SRC_ALPHA,
        .dst = QB_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      },
    };

    qbViewport_ viewport{
      .x = 0,
      .y = 0,
      .w = (float)qb_window_width(),
      .h = (float)qb_window_height(),
      .min_depth = 0.f,
      .max_depth = 1.f
    };

    qbRect_ scissor{
      .x = 0,
      .y = 0,
      .w = (float)qb_window_width(),
      .h = (float)qb_window_height()
    };

    qbViewportState_ viewport_state{
      .viewport = viewport,
      .scissor = scissor
    };

    qbRenderPipelineAttr_ attr{
      .shader = shader,
      .geometry = geometry,
      .render_pass = render_pass,
      .blend_state = &blend_state,
      .viewport_state = &viewport_state,
      .rasterization_info = &raster_info,
      .resource_layout = resource_pipeline_layout
    };

    qb_renderpipeline_create(&pipeline, &attr);
  }

  return pipeline;
}

void create_resourcesets(qbDefaultRenderer r) {
  size_t swapchain_size = r->swapchain_images.size();

  r->draw_allocator = qb_memallocator_pool();
  r->cmd_buf_allocator = qb_memallocator_linear(1 << 20);
  r->cmd_bufs = std::vector<qbDrawCommandBuffer>(swapchain_size, nullptr);
  {
    qbDrawCommandBufferAttr_ attr{
      .count = 2,
      .allocator = r->cmd_buf_allocator,
    };

    qb_drawcmd_create(r->cmd_bufs.data(), &attr);
  }

  r->resource_sets = std::vector<qbShaderResourceSet>(swapchain_size, nullptr);
  {
    qbShaderResourceSetAttr_ attr{
      .create_count = (uint32_t)swapchain_size,
      .layout = r->resource_layout
    };
    qb_shaderresourceset_create(r->resource_sets.data(), &attr);

    for (size_t i = 0; i < swapchain_size; ++i) {
      qb_shaderresourceset_writeuniform(r->resource_sets[i], 0, r->camera_ubo);
      qb_shaderresourceset_writeuniform(r->resource_sets[i], 1, r->material_ubo);
      qb_shaderresourceset_writeuniform(r->resource_sets[i], 2, r->model_ubo);
    }
  }
}

void create_vbos(qbDefaultRenderer r) {
  for (size_t i = 0; i < r->swapchain_images.size(); ++i) {
    qbGpuBufferAttr_ attr{
      .data = nullptr,
      .size = sizeof(Vertex) * MAX_TRI_BATCH_COUNT,
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&tri_verts, &attr);
  }

  {
    Vertex vertex_data[] = {
      {.pos = {0.0f, 0.0f, 0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 0.f}},
      {.pos = {1.0f, 1.0f, 0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 1.f}},
      {.pos = {1.0f, 0.0f, 0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 0.f}},
      {.pos = {0.0f, 0.0f, 0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 0.f}},
      {.pos = {0.0f, 1.0f, 0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 1.f}},
      {.pos = {1.0f, 1.0f, 0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 1.f}},
    };

    qbGpuBufferAttr_ attr{
      .data = vertex_data,
      .size = sizeof(vertex_data),
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&quad_verts, &attr);
  }

  {
    std::vector<Vertex> vertex_data;
    float r = .5f;
    vec2s center = { 0.f, 0.f };
    for (int i = 0; i < 64; i += 1) {
      float dir = 2.f * GLM_PI * (float)i / 64.f;
      float dir_next = 2.f * GLM_PI * (float)(i + 1) / 64.f;

      float x = r * cos(dir);
      float y = r * sin(dir);

      float x_next = r * cos(dir_next);
      float y_next = r * sin(dir_next);

      vertex_data.push_back({
        .pos = {center.x, center.y, 0.f},
        .norm = {},
        .tex = {}
      });

      vertex_data.push_back({
        .pos = {center.x - x_next, center.y - y_next, 0.f},
        .norm = {},
        .tex = {}
      });

      vertex_data.push_back({
        .pos = {center.x - x, center.y - y, 0.f},
        .norm = {},
        .tex = {}
      });
    }

    qbGpuBufferAttr_ attr{
      .data = vertex_data.data(),
      .size = vertex_data.size() * sizeof(Vertex),
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&circle_verts, &attr);
    circle_verts_count = vertex_data.size();
  }

  {
    Vertex vertex_data[] = {
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},

      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
    };

    box_verts_count = sizeof(vertex_data) / sizeof(Vertex);

    qbGpuBufferAttr_ attr{
      .data = vertex_data,
      .size = sizeof(vertex_data),
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&box_verts, &attr);
  }
}

void record_command_buffers(qbDefaultRenderer r, uint32_t frame) {
  size_t swapchain_size = r->swapchain_images.size();

  qbDrawCommandBuffer draw_cmds = r->cmd_bufs[frame];

  qbClearValue_ clear_values[2] = {};
  clear_values[0].color = { 0.f, 0.f, 0.f, 1.f };
  clear_values[1].depth = 1.f;

  qbBeginRenderPassInfo_ begin_info{
    .render_pass = r->render_pass,
    .framebuffer = r->fbos[frame],
    .clear_values = clear_values,
    .clear_values_count = 2,
  };

  qb_drawcmd_beginpass(draw_cmds, &begin_info);
  qb_drawcmd_bindpipeline(draw_cmds, r->render_pipeline);
  qb_drawcmd_bindshaderresourceset(draw_cmds, r->pipeline_layout, 1, &r->resource_sets[frame]);  

  for (auto& [material, cmds] : r->draw_commands.commands) {
    if (material) {
      qb_gpubuffer_update(r->material_ubo, 0, sizeof(MaterialUbo), material);
    }

    for (auto& draw_cmd : cmds) {
      if (!material) {
        qbMaterial_ default_mat{};
        default_mat.albedo = { draw_cmd.args.color.x, draw_cmd.args.color.y, draw_cmd.args.color.z };
        qb_drawcmd_pushbuffer(draw_cmds, r->material_ubo, 0, sizeof(MaterialUbo), &default_mat);
      }

      switch (draw_cmd.type) {
      case QB_DRAW_CAMERA:
      {
        const qbCamera_* camera = draw_cmd.command.init.camera;
        r->camera_data[frame].viewproj = glms_mat4_mul(camera->projection_mat, camera->view_mat);
        qb_gpubuffer_update(r->camera_ubo, 0, sizeof(CameraUbo), &r->camera_data[frame]);
        break;
      }

      case QB_DRAW_TRI:
        qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &tri_verts);
        qb_drawcmd_draw(draw_cmds, 3, 1, 0, 0);
        break;

      case QB_DRAW_QUAD:
        qb_drawcmd_updatebuffer(draw_cmds, r->model_ubo, 0, sizeof(ModelUbo), &draw_cmd.args.transform);
        qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &quad_verts);
        qb_drawcmd_draw(draw_cmds, 6, 1, 0, 0);
        break;

      case QB_DRAW_CIRCLE:
        qb_drawcmd_updatebuffer(draw_cmds, r->model_ubo, 0, sizeof(ModelUbo), &draw_cmd.args.transform);
        qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &circle_verts);
        qb_drawcmd_draw(draw_cmds, circle_verts_count, 1, 0, 0);
        break;

      case QB_DRAW_BOX:
        qb_drawcmd_updatebuffer(draw_cmds, r->model_ubo, 0, sizeof(ModelUbo), &draw_cmd.args.transform);
        qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &box_verts);
        qb_drawcmd_draw(draw_cmds, circle_verts_count, 1, 0, 0);
        break;
      }
    }
  }

  qb_drawcmd_endpass(draw_cmds);
}

void render(struct qbRenderer_* self, qbRenderEvent event) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;


  uint32_t frame = qb_swapchain_waitforframe(r->swapchain);

  CameraUbo camera{};
  camera.frame = (float)event->frame;
  qb_gpubuffer_update(r->camera_ubo, 0, sizeof(CameraUbo), &camera);

  record_command_buffers(r, frame);

  qbDrawPresentInfo_ present_info{
    .swapchain = r->swapchain,
    .image_index = frame
  };
  qb_drawcmd_present(r->cmd_bufs[frame], &present_info);
  r->draw_commands.commands.clear();
}

qbResult drawcommands_submit(struct qbRenderer_* self, struct qbDrawCommands_* cmd_buf) {
  size_t cmd_count = 0;
  qbDrawCommand cmds;

  qb_draw_commands(cmd_buf, &cmd_count, &cmds);
  for (size_t i = 0; i < cmd_count; ++i) {
    ((qbDefaultRenderer)self)->draw_commands.push(std::move(cmds[i]));
  }

  return QB_OK;
}

void light_enable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < kMaxDirectionalLights) {
      r->enabled_directional_lights[id] = true;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < kMaxPointLights) {
      r->enabled_point_lights[id] = true;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < kMaxSpotLights) {
      r->enabled_spot_lights[id] = true;
    }
    break;
  }
}

void light_disable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < kMaxDirectionalLights) {
      r->enabled_directional_lights[id] = false;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < kMaxPointLights) {
      r->enabled_point_lights[id] = false;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < kMaxSpotLights) {
      r->enabled_spot_lights[id] = false;
    }
    break;
  }
}

bool light_isenabled(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < kMaxDirectionalLights) {
      return r->enabled_directional_lights[id];
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < kMaxPointLights) {
      return r->enabled_point_lights[id];
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < kMaxSpotLights) {
      return r->enabled_spot_lights[id];
    }
    break;
  }
  return false;
}

void light_directional(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s dir, float brightness) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  if (id >= kMaxDirectionalLights) {
    return;
  }

  r->directional_lights[id] = LightDirectional{ rgb, brightness, dir };
}

void light_point(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s pos, float brightness, float range) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  if (id >= kMaxPointLights) {
    return;
  }

  r->point_lights[id] = LightPoint{ rgb, brightness, pos, range };
}

void light_spot(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s pos, vec3s dir,
  float brightness, float range, float angle_deg) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  if (id >= kMaxSpotLights) {
    return;
  }

  r->spot_lights[id] = LightSpot{ rgb, brightness, pos, range, dir, angle_deg };
}

size_t light_max(struct qbRenderer_* self, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;

  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    return kMaxDirectionalLights;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    return kMaxPointLights;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    return kMaxSpotLights;
  }

  return 0;
}


struct qbRenderer_* qb_defaultrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args) {
  qbDefaultRenderer ret = new qbDefaultRenderer_{};
  ret->renderer.drawcommands_submit = drawcommands_submit;
  ret->renderer.render = render;
  ret->renderer.light_enable = light_enable;
  ret->renderer.light_disable = light_disable;
  ret->renderer.light_isenabled = light_isenabled;
  ret->renderer.light_directional = light_directional;
  ret->renderer.light_point = light_point;
  ret->renderer.light_spot = light_spot;
  ret->renderer.light_max = light_max;

  ret->geometry = create_geometry_descriptor();
  ret->camera_ubo = create_camera_uniform();
  ret->model_ubo = create_model_uniform();
  ret->material_ubo = create_material_uniform();
  ret->resource_layout = create_resource_layout();
  ret->pipeline_layout = create_resource_pipeline_layout(ret->resource_layout);
  ret->shader_module = create_shader_module();
  ret->render_pass = create_renderpass();

  create_swapchain(width, height, ret->render_pass, ret);

  ret->render_pipeline = create_render_pipeline(
    ret->shader_module, ret->geometry, ret->render_pass, ret->pipeline_layout);

  create_vbos(ret);
  create_resourcesets(ret);

  return (qbRenderer)ret;
}

void qb_defaultrenderer_destroy(qbRenderer renderer) {
  qbDefaultRenderer r = (qbDefaultRenderer)renderer;
  delete (qbDefaultRenderer)renderer;
}