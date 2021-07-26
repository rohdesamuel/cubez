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
  vec3s pos[3];
  vec3s norm[3];
  vec2s tex[2];
};

struct LightPoint {
  // Interleave the brightness and radius to optimize std140 wasted bytes.
  vec3s pos;
  float linear;
  vec3s col;
  float quadratic;
  float radius;

  // The struct needs to be a multiple of the size of vec4 (std140).
  float pad_[3];
};

struct LightDirectional {
  vec3s col;
  float brightness;
  vec3s dir;

  // The struct needs to be a multiple of the size of vec4 (std140).
  float _pad[1];
};

struct LightSpot {
  vec3s col;
  float brightness;
  vec3s pos;
  float range;
  vec3s dir;
  float angle_deg;
};

const int MAX_DIRECTIONAL_LIGHTS = 4;
const int MAX_POINT_LIGHTS = 32;
const int MAX_SPOT_LIGHTS = 8;

struct LightUbo {
  LightPoint point_lights[MAX_POINT_LIGHTS];
  LightDirectional dir_lights[MAX_DIRECTIONAL_LIGHTS];
  vec3s view_pos;
  float num_point_lights;
  float num_dir_lights;

  // The struct needs to be a multiple of the size of vec4 (std140).
  float _pad[3];
};

struct DrawCommandQueue {
  void push(qbDrawCommand_&& command) {
    commands[command.args.material].push_back(std::move(command));
  }

  std::unordered_map<qbMaterial, std::vector<qbDrawCommand_>> commands;
};

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
  qbGpuBuffer light_ubo;

  std::array<struct LightDirectional, MAX_DIRECTIONAL_LIGHTS> directional_lights;
  std::array<bool, MAX_DIRECTIONAL_LIGHTS> enabled_directional_lights;
  std::array<struct LightPoint, MAX_POINT_LIGHTS> point_lights;
  std::array<bool, MAX_POINT_LIGHTS> enabled_point_lights;
  std::array<struct LightSpot, MAX_SPOT_LIGHTS> spot_lights;
  std::array<bool, MAX_SPOT_LIGHTS> enabled_spot_lights;

  // Render queue.
  std::vector<qbTask> render_tasks;
  std::vector<qbClearValue_> clear_values;
  std::vector<qbDrawCommandBuffer> cmd_bufs;
  std::vector<qbShaderResourceSet> resource_sets;
  std::vector<qbFrameBuffer> fbos;
  std::vector<qbImage> swapchain_images;
  std::vector<CameraUbo> camera_data;

  DrawCommandQueue draw_commands;

  // Deferred Rendering fields.
  // Deferred G-Buffer.
  qbImage gposition_buffer;
  qbImage gnormal_buffer;
  qbImage galbedo_spec_buffer;

  std::vector<qbImage> gbuffers;
  std::vector<qbImageSampler> gbuffer_samplers;

  qbShaderResourceLayout deferred_resource_layout;
  qbShaderResourcePipelineLayout deferred_pipeline_layout;
  qbShaderModule deferred_shader_module;
  qbRenderPass deferred_render_pass;
  qbRenderPipeline deferred_render_pipeline;
  std::vector<qbShaderResourceSet> deferred_resource_sets;

  std::vector<qbFrameBuffer> deferred_fbos;

};

qbGpuBuffer tri_verts;
qbGpuBuffer quad_verts;
qbGpuBuffer screenquad_verts;
qbGpuBuffer circle_verts;
size_t circle_verts_count;

qbGpuBuffer box_verts;
size_t box_verts_count;

qbGpuBuffer sphere_verts;
size_t sphere_verts_count;

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

template<class Ty_>
qbGpuBuffer create_uniform(Ty_ material_data) {
  qbGpuBufferAttr_ attr{
    .data = &material_data,
    .size = sizeof(Ty_),
    .buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM,
  };

  qbGpuBuffer ret;
  qb_gpubuffer_create(&ret, &attr);
  return ret;
}

template<class Ty_>
qbGpuBuffer create_uniform() {
  Ty_ material_data{};
  return create_uniform(material_data);
}

qbShaderResourceLayout create_resource_layout() {
  qbShaderResourceLayout resource_layout;
  qbShaderResourceBinding_ bindings[] = {
    {
      .name = "light_ubo",
      .binding = 0,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_FRAGMENT,
    },
    {
      .name = "gPosition",
      .binding = 0,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "gNormal",
      .binding = 1,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "gAlbedoSpec",
      .binding = 2,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
  };

  qbShaderResourceLayoutAttr_ attr{
    .binding_count = sizeof(bindings) / sizeof(qbShaderResourceBinding_),
    .bindings = bindings,
  };

  qb_shaderresourcelayout_create(&resource_layout, &attr);

  return resource_layout;
}

qbShaderResourceLayout create_deferred_resource_layout() {
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
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNorm;
    layout (location = 2) in vec2 aTexCoords;

    out vec2 TexCoords;

    void main()
    {
        TexCoords = aTexCoords;
        gl_Position = vec4(aPos, 1.0);
    })";

  attr.fs = R"(
    #version 330 core

    layout (location = 0) out vec4 out_color;

    in vec2 TexCoords;

    uniform sampler2D gPosition;
    uniform sampler2D gNormal;
    uniform sampler2D gAlbedoSpec;

    struct LightPoint {
      vec3 pos;
      float linear;
      vec3 col;
      float quadratic;
      float radius;
    };

    struct LightDirectional {
      vec3 col;
      float brightness;
      vec3 dir;
    };

    const int MAX_POINT_LIGHTS = 32;
    const int MAX_DIRECTIONAL_LIGHTS = 4;
    const int MAX_SPOT_LIGHTS = 8;

    layout(std140, binding = 0) uniform LightUbo {
      LightPoint point_lights[MAX_POINT_LIGHTS];
      LightDirectional dir_lights[MAX_DIRECTIONAL_LIGHTS];
      vec3 view_pos;
      float num_point_lights;
    } light_ubo;

    void main()
    {   
        // retrieve data from gbuffer
        vec3 FragPos = texture(gPosition, TexCoords).rgb;
        vec3 Normal = texture(gNormal, TexCoords).rgb;
        vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;

        // TODO: implement specular
        float Specular = texture(gAlbedoSpec, TexCoords).a;
    
        // then calculate lighting as usual
        vec3 lighting  = Diffuse * 0.1; // hard-coded ambient component
        vec3 viewDir  = normalize(light_ubo.view_pos - FragPos);
        for(int i = 0; i < light_ubo.num_point_lights; ++i)
        {
            // calculate distance between light source and current fragment
            float distance = length(light_ubo.point_lights[i].pos - FragPos);
            
            if(distance < light_ubo.point_lights[i].radius)
            {
                // diffuse
                vec3 lightDir = normalize(light_ubo.point_lights[i].pos - FragPos);
                vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light_ubo.point_lights[i].col;
                // specular
                vec3 halfwayDir = normalize(lightDir + viewDir);  
                float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
                vec3 specular = light_ubo.point_lights[i].col * spec * Specular;
                // attenuation
                float attenuation = 1.0 / (1.0 + light_ubo.point_lights[i].linear * distance + light_ubo.point_lights[i].quadratic * distance * distance);
                diffuse *= attenuation;
                specular *= attenuation;
                lighting += (diffuse + specular);
            }
        }    

        // TODO: uncomment
        out_color = vec4(lighting, 1.0);
    })";

  attr.interpret_as_strings = QB_TRUE;

  qb_shadermodule_create(&shader, &attr);

  return shader;
}

qbShaderModule create_deferred_shader_module() {
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
      vec3 pos;
      vec3 norm;
      vec2 tex;
    } o;

    void main() {
      vec4 world_pos = model_ubo.model * vec4(in_pos, 1.0);
      mat3 normal_matrix = transpose(inverse(mat3(model_ubo.model)));

      o.pos = world_pos.xyz;
      o.norm = normal_matrix * in_norm;
      o.tex = in_tex;

      gl_Position = camera_ubo.viewproj * world_pos;
    })";

  attr.fs = R"(
    #version 330 core

    layout (location = 0) out vec3 g_position;
    layout (location = 1) out vec3 g_normal;
    layout (location = 2) out vec4 g_albedospec;

    layout (std140, binding = 1) uniform MaterialUbo {
      vec3 albedo;
      float metallic;
      vec3 emission;
      float roughness;
    } material_ubo;

    in VertexData
    {
      vec3 pos;
	    vec3 norm;
      vec2 tex;
    } o;

    void main() {
      g_position = o.pos;
      g_normal = normalize(o.norm);
      g_albedospec = vec4(material_ubo.albedo, material_ubo.metallic);
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

  {
    qbImageAttr_ attr{ .type = QB_IMAGE_TYPE_2D };
    qb_image_raw(
      &r->gposition_buffer, &attr, QB_PIXEL_FORMAT_RGB16F,
      qb_window_width(), qb_window_height(), nullptr);
  }

  {
    qbImageAttr_ attr{ .type = QB_IMAGE_TYPE_2D };
    qb_image_raw(
      &r->gnormal_buffer, &attr, QB_PIXEL_FORMAT_RGB16F,
      qb_window_width(), qb_window_height(), nullptr);
  }

  {
    qbImageAttr_ attr{ .type = QB_IMAGE_TYPE_2D };
    qb_image_raw(
      &r->galbedo_spec_buffer, &attr, QB_PIXEL_FORMAT_RGBA16F,
      qb_window_width(), qb_window_height(), nullptr);
  }
  
  r->gbuffers = { r->gposition_buffer, r->gnormal_buffer, r->galbedo_spec_buffer };
  r->gbuffer_samplers = std::vector<qbImageSampler>(r->gbuffers.size(), nullptr);
  for (size_t i = 0; i < r->gbuffers.size(); ++i) {
    qbImageSamplerAttr_ attr{};
    attr.mag_filter = QB_FILTER_TYPE_NEAREST;
    attr.min_filter = QB_FILTER_TYPE_NEAREST;
    qb_imagesampler_create(&r->gbuffer_samplers[i], &attr);
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

  r->deferred_fbos = std::vector<qbFrameBuffer>(swapchain_image_count, nullptr);
  for (size_t i = 0; i < swapchain_image_count; ++i) {
    // TODO: replace with gposition_buffer
    qbFramebufferAttachment_ attachments[] = {
      { r->gposition_buffer, QB_COLOR_ASPECT },
      { r->gnormal_buffer, QB_COLOR_ASPECT },
      { r->galbedo_spec_buffer, QB_COLOR_ASPECT },
      { r->depth_image, QB_DEPTH_STENCIL_ASPECT }
    };

    qbFrameBufferAttr_ attr{
      .render_pass = r->deferred_render_pass,
      .attachments = attachments,
      .attachments_count = 4,
      .width = width,
      .height = height,
    };

    qb_framebuffer_create(&r->deferred_fbos[i], &attr);
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

qbRenderPass create_deferred_renderpass() {
  qbRenderPass render_pass;
  qbFramebufferAttachmentRef_ attachments[4] = {};
  attachments[0] = {
    .attachment = 0,
    .aspect = qbImageAspect::QB_COLOR_ASPECT
  };
  attachments[1] = {
    .attachment = 1,
    .aspect = qbImageAspect::QB_COLOR_ASPECT
  };
  attachments[2] = {
    .attachment = 2,
    .aspect = qbImageAspect::QB_COLOR_ASPECT
  };
  attachments[3] = {
    .attachment = 3,
    .aspect = qbImageAspect::QB_DEPTH_STENCIL_ASPECT
  };

  qbRenderPassAttr_ attr{
    .attachments = attachments,
    .attachments_count = 4
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
      .cull_face = QB_FACE_NONE,
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
      qb_shaderresourceset_writeuniform(r->resource_sets[i], 0, r->light_ubo);
      for (size_t j = 0; j < r->gbuffer_samplers.size(); ++j) {
        qb_shaderresourceset_writeimage(r->resource_sets[i], j, r->gbuffers[j], r->gbuffer_samplers[j]);
      }
    }
  }
}

void create_deferred_resourcesets(qbDefaultRenderer r) {
  size_t swapchain_size = r->swapchain_images.size();

  r->deferred_resource_sets = std::vector<qbShaderResourceSet>(swapchain_size, nullptr);
  {
    qbShaderResourceSetAttr_ attr{
      .create_count = (uint32_t)swapchain_size,
      .layout = r->deferred_resource_layout
    };
    qb_shaderresourceset_create(r->deferred_resource_sets.data(), &attr);

    for (size_t i = 0; i < swapchain_size; ++i) {
      qb_shaderresourceset_writeuniform(r->deferred_resource_sets[i], 0, r->camera_ubo);
      qb_shaderresourceset_writeuniform(r->deferred_resource_sets[i], 1, r->material_ubo);
      qb_shaderresourceset_writeuniform(r->deferred_resource_sets[i], 2, r->model_ubo);
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
    Vertex vertex_data[] = {
      {.pos = {-1.0f, -1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 1.f}},
      {.pos = { 1.0f, -1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 1.f}},
      {.pos = { 1.0f,  1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 1.f}},
    };

    qbGpuBufferAttr_ attr{
      .data = vertex_data,
      .size = sizeof(vertex_data),
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&screenquad_verts, &attr);
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
        .norm = {0.f, 0.f, 1.f},
        .tex = {}
        });

      vertex_data.push_back({
        .pos = {center.x - x_next, center.y - y_next, 0.f},
        .norm = {0.f, 0.f, 1.f},
        .tex = {}
        });

      vertex_data.push_back({
        .pos = {center.x - x, center.y - y, 0.f},
        .norm = {0.f, 0.f, 1.f},
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
    std::vector<Vertex> vertex_data;
    float radius = .5f;
    float slices = 22.f;
    float zslices = 22.f;
    float zdir_step = 180.0f / zslices;
    float dir_step = 360.0f / slices;
    for (float zdir = 0; zdir < 180; zdir += zdir_step) {
      for (float dir = 0; dir < 360; dir += dir_step) {
        float zdir_rad_t = glm_rad(zdir);
        float zdir_rad_d = glm_rad(zdir + zdir_step);
        float dir_rad_l = glm_rad(dir);
        float dir_rad_r = glm_rad(dir + dir_step);

        vec3s p0 = {
          sin(zdir_rad_t) * cos(dir_rad_l),
          sin(zdir_rad_t) * sin(dir_rad_l),
          cos(zdir_rad_t)
        };
        vec2s t0 = {
          dir_rad_l / (2.0f * GLM_PIf),
          zdir_rad_t / GLM_PIf
        };
        p0 = glms_vec3_scale(p0, radius);

        vec3s p1 = {
          sin(zdir_rad_t) * cos(dir_rad_r),
          sin(zdir_rad_t) * sin(dir_rad_r),
          cos(zdir_rad_t)
        };
        vec2s t1 = {
          dir_rad_r / (2.0f * GLM_PIf),
          zdir_rad_t / GLM_PIf
        };
        p1 = glms_vec3_scale(p1, radius);

        vec3s p2 = {
          sin(zdir_rad_d) * cos(dir_rad_r),
          sin(zdir_rad_d) * sin(dir_rad_r),
          cos(zdir_rad_d)
        };
        vec2s t2 = {
          dir_rad_r / (2.0f * GLM_PIf),
          zdir_rad_d / GLM_PIf
        };
        p2 = glms_vec3_scale(p2, radius);

        vec3s p3 = {
          sin(zdir_rad_d) * cos(dir_rad_l),
          sin(zdir_rad_d) * sin(dir_rad_l),
          cos(zdir_rad_d)
        };
        vec2s t3 = {
          dir_rad_l / (2.0f * GLM_PIf),
          zdir_rad_d / GLM_PIf
        };
        p3 = glms_vec3_scale(p3, radius);

        if (zdir > 0) {
          vertex_data.push_back(Vertex{
            .pos = p2,
            .norm = glms_vec3_scale(p2, 1.0f / radius),
            .tex = t2
          });

          vertex_data.push_back(Vertex{
            .pos = p1,
            .norm = glms_vec3_scale(p1, 1.0f / radius),
            .tex = t1
          });

          vertex_data.push_back(Vertex{
            .pos = p0,
            .norm = glms_vec3_scale(p0, 1.0f / radius),
            .tex = t0
          });
        }

        vertex_data.push_back(Vertex{
          .pos = p2,
          .norm = glms_vec3_scale(p2, 1.0f / radius),
          .tex = t2
        });

        vertex_data.push_back(Vertex{
          .pos = p0,
          .norm = glms_vec3_scale(p0, 1.0f / radius),
          .tex = t0
        });

        vertex_data.push_back(Vertex{
          .pos = p3,
          .norm = glms_vec3_scale(p3, 1.0f / radius),
          .tex = t3
        });
      }
    }


    qbGpuBufferAttr_ attr{
      .data = vertex_data.data(),
      .size = vertex_data.size() * sizeof(Vertex),
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&sphere_verts, &attr);
    sphere_verts_count = vertex_data.size();
  }

  {
    Vertex vertex_data[] = {
      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = {-1.f,  0.f,  0.f}, .tex = {0.f, 0.f}},

      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f,  0.f,  1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f,  0.f, -1.f}, .tex = {0.f, 0.f}},

      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f, -1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f, -1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f,  1.0f,  1.0f}, .norm = { 0.f,  1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f, -1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f, -1.0f,  1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f,  1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
      {.pos = {-1.0f, -1.0f, -1.0f}, .norm = { 0.f, -1.f,  0.f}, .tex = {0.f, 0.f}},
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

void update_light_ubo(qbDefaultRenderer r, const struct qbCamera_* camera) {
  LightUbo l = {};
  for (size_t i = 0; i < r->directional_lights.size(); ++i) {
    if (r->enabled_directional_lights[i]) {
      l.dir_lights[i] = r->directional_lights[i];
    }
  }

  int enabled_index = 0;
  for (size_t i = 0; i < r->point_lights.size(); ++i) {
    if (r->enabled_point_lights[i]) {
      l.point_lights[enabled_index] = r->point_lights[i];
      ++enabled_index;
    }
  }

  l.view_pos = camera->eye;
  l.num_point_lights = (float)enabled_index;

  qb_gpubuffer_update(r->light_ubo, 0, sizeof(LightUbo), &l);
}

void record_command_buffers(qbDefaultRenderer r, uint32_t frame) {
  size_t swapchain_size = r->swapchain_images.size();

  qbDrawCommandBuffer draw_cmds = r->cmd_bufs[frame];

  {
    qbClearValue_ clear_values[4] = {};
    clear_values[0].color = { 0.f, 0.f, 0.f, 0.f };
    clear_values[1].color = { 0.f, 0.f, 0.f, 0.f };
    clear_values[2].color = { 0.f, 0.f, 0.f, 0.f };
    clear_values[3].depth = 1.f;

    
    qbBeginRenderPassInfo_ begin_info{
      .render_pass = r->deferred_render_pass,
      .framebuffer = r->deferred_fbos[frame],
      .clear_values = clear_values,
      .clear_values_count = 4,
    };

    qb_drawcmd_beginpass(draw_cmds, &begin_info);
    qb_drawcmd_bindpipeline(draw_cmds, r->deferred_render_pipeline);
    qb_drawcmd_bindshaderresourceset(draw_cmds, r->deferred_pipeline_layout, 1, &r->deferred_resource_sets[frame]);

    for (auto& [material, cmds] : r->draw_commands.commands) {
      if (material) {
        qb_drawcmd_updatebuffer(draw_cmds, r->material_ubo, 0, sizeof(MaterialUbo), material);
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
          update_light_ubo(r, camera);
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
          qb_drawcmd_draw(draw_cmds, box_verts_count, 1, 0, 0);
          break;

        case QB_DRAW_SPHERE:
          qb_drawcmd_updatebuffer(draw_cmds, r->model_ubo, 0, sizeof(ModelUbo), &draw_cmd.args.transform);
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &sphere_verts);
          qb_drawcmd_draw(draw_cmds, sphere_verts_count, 1, 0, 0);
          break;
        }
      }
    }

    qb_drawcmd_endpass(draw_cmds);
  }

  {
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
    qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &screenquad_verts);
    qb_drawcmd_draw(draw_cmds, 6, 1, 0, 0);
    qb_drawcmd_endpass(draw_cmds);
  }
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
    if (id < MAX_DIRECTIONAL_LIGHTS) {
      r->enabled_directional_lights[id] = true;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < MAX_POINT_LIGHTS) {
      r->enabled_point_lights[id] = true;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < MAX_SPOT_LIGHTS) {
      r->enabled_spot_lights[id] = true;
    }
    break;
  }
}

void light_disable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < MAX_DIRECTIONAL_LIGHTS) {
      r->enabled_directional_lights[id] = false;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < MAX_POINT_LIGHTS) {
      r->enabled_point_lights[id] = false;
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < MAX_SPOT_LIGHTS) {
      r->enabled_spot_lights[id] = false;
    }
    break;
  }
}

bool light_isenabled(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < MAX_DIRECTIONAL_LIGHTS) {
      return r->enabled_directional_lights[id];
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < MAX_POINT_LIGHTS) {
      return r->enabled_point_lights[id];
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < MAX_SPOT_LIGHTS) {
      return r->enabled_spot_lights[id];
    }
    break;
  }
  return false;
}

void light_directional(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s dir, float brightness) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  if (id >= MAX_DIRECTIONAL_LIGHTS) {
    return;
  }

  r->directional_lights[id] = LightDirectional{ rgb, brightness, dir };
}

void light_point(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s pos, float linear, float quadratic, float radius) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  if (id >= MAX_POINT_LIGHTS) {
    return;
  }

  r->point_lights[id] = LightPoint{ .pos=pos, .linear=linear, .col=rgb, .quadratic=quadratic, .radius=radius };
}

void light_spot(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s pos, vec3s dir,
  float brightness, float range, float angle_deg) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  if (id >= MAX_SPOT_LIGHTS) {
    return;
  }

  r->spot_lights[id] = LightSpot{ rgb, brightness, pos, range, dir, angle_deg };
}

size_t light_max(struct qbRenderer_* self, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;

  switch (light_type) {
    case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
      return MAX_DIRECTIONAL_LIGHTS;
    case qbLightType::QB_LIGHT_TYPE_POINT:
      return MAX_POINT_LIGHTS;
    case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
      return MAX_SPOT_LIGHTS;
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
  ret->camera_ubo = create_uniform<CameraUbo>();
  ret->model_ubo = create_uniform<ModelUbo>();
  ret->material_ubo = create_uniform<MaterialUbo>();
  ret->light_ubo = create_uniform<LightUbo>();

  ret->resource_layout = create_resource_layout();
  ret->pipeline_layout = create_resource_pipeline_layout(ret->resource_layout);
  ret->shader_module = create_shader_module();
  ret->render_pass = create_renderpass();

  ret->deferred_resource_layout = create_deferred_resource_layout();
  ret->deferred_pipeline_layout = create_resource_pipeline_layout(ret->deferred_resource_layout);
  ret->deferred_shader_module = create_deferred_shader_module();
  ret->deferred_render_pass = create_deferred_renderpass();

  create_swapchain(width, height, ret->render_pass, ret);

  ret->render_pipeline = create_render_pipeline(
    ret->shader_module, ret->geometry, ret->render_pass, ret->pipeline_layout);

  ret->deferred_render_pipeline = create_render_pipeline(
    ret->deferred_shader_module, ret->geometry, ret->deferred_render_pass, ret->deferred_pipeline_layout);

  create_vbos(ret);
  create_resourcesets(ret);
  create_deferred_resourcesets(ret);

  return (qbRenderer)ret;
}

void qb_defaultrenderer_destroy(qbRenderer renderer) {
  qbDefaultRenderer r = (qbDefaultRenderer)renderer;
  delete (qbDefaultRenderer)renderer;
}