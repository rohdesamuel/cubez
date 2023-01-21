#if 0
#include <cubez/renderer.h>

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cubez/mesh.h>
#include <cubez/render.h>
#include <cubez/draw.h>
#include <cubez/memory.h>
#include <cubez/log.h>

#include <cglm/struct.h>
#include <array>
#include <unordered_map>
#include <unordered_set>

const size_t MAX_TRI_BATCH_COUNT = 100;

struct CameraUbo {
  mat4s viewproj;
  vec3s eye;
  float frame;
};

struct ModelUbo {
  mat4s parent;
  mat4s model;
};

struct MaterialUbo {
  float albedo[3];
  float metallic;
  float emission[3];
  float roughness;
};

struct Vertex {
  vec3s pos;
  vec3s norm;
  vec2s tex;
};

struct LightPoint {
  // Interleave the brightness and radius to optimize std140 wasted bytes.
  vec3s pos;
  float linear;
  vec3s col;
  float quadratic;
  float radius;

  // The struct needs to be a multiple of the size of vec4 (std140).
  float _pad[3];
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
const int MAX_POINT_LIGHTS = 1024;
const int MAX_SPOT_LIGHTS = 8;

struct LightUbo {
  LightPoint point_lights[MAX_POINT_LIGHTS];
  LightDirectional dir_lights[MAX_DIRECTIONAL_LIGHTS];
  int32_t pass_id;
  int32_t num_dir_lights;
  // The struct needs to be a multiple of the size of vec4 (std140).
  float _pad[2];
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
  qbMemoryAllocator dynamic_cmd_buf_allocator;
  qbMemoryAllocator static_cmd_buf_allocator;
  qbMemoryAllocator lighting_cmd_buf_allocator;
  qbMemoryAllocator draw_allocator;
  qbMemoryAllocator allocator;

  qbGpuBuffer quad_buffer;
  qbGpuBuffer camera_ubo;
  qbGpuBuffer material_ubo;
  qbGpuBuffer model_ubo;

  std::array<struct LightDirectional, MAX_DIRECTIONAL_LIGHTS> directional_lights;
  std::unordered_set<qbId> enabled_directional_lights;
  std::array<struct LightPoint, MAX_POINT_LIGHTS> point_lights;
  std::unordered_set<qbId> enabled_point_lights;
  std::array<struct LightSpot, MAX_SPOT_LIGHTS> spot_lights;
  std::unordered_set<qbId> enabled_spot_lights;

  std::unordered_map<qbMaterial, qbShaderResourceSet> material_resource_sets;
  qbShaderResourceLayout material_resource_layout;

  // Render queue.
  std::vector<qbTask> render_tasks;
  std::vector<qbClearValue_> clear_values;
  std::vector<qbDrawCommand_> static_cmd_bufs;
  std::vector<qbDrawCommandBuffer> dynamic_cmd_bufs;
  std::vector<qbDrawCommandBuffer> lighting_cmd_bufs;

  std::vector<qbShaderResourceSet> resource_sets;
  std::vector<qbFrameBuffer> fbos;
  std::vector<qbImage> swapchain_images;
  std::vector<CameraUbo> camera_data;

  // Todo: consider adding ability to render to an off-screen buffer.
  const qbCamera_* camera;
  qbClearValue_ clear_value;

  DrawCommandQueue draw_commands;

  // Deferred Rendering fields.
  // Deferred G-Buffer.
  qbImage gposition_buffer;
  qbImage gnormal_buffer;
  qbImage galbedo_spec_buffer;

  std::vector<qbImage> gbuffers;
  std::vector<qbImageSampler> gbuffer_samplers;
  std::vector<qbImage> default_textures;
  std::vector<qbImageSampler> deferred_samplers;

  qbShaderResourceLayout deferred_resource_layout;
  qbShaderResourcePipelineLayout deferred_pipeline_layout;
  qbShaderModule deferred_shader_module;
  qbRenderPass deferred_render_pass;
  qbRenderPipeline deferred_render_pipeline;
  std::vector<qbShaderResourceSet> deferred_resource_sets;

  std::vector<qbFrameBuffer> deferred_fbos;

  // Light state.
  LightUbo* light_ubo_buf;
  qbGpuBuffer light_ubo;
};

qbGpuBuffer tri_verts;
qbGpuBuffer quad_verts;
qbGpuBuffer screenquad_verts;

qbGpuBuffer circle_verts;
size_t circle_verts_count;

qbGpuBuffer box_verts;
size_t box_verts_count;

std::vector<Vertex> sphere_vertices;
qbGpuBuffer sphere_verts;
size_t sphere_verts_count;

qbImage white_tex;
qbImage black_tex;
qbImage zero_tex;

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
qbGpuBuffer create_uniform() {
  qbGpuBufferAttr_ attr{
    .data = nullptr,
    .size = sizeof(Ty_),
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
      .name = "light_ubo",
      .binding = 1,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_VERTEX | QB_SHADER_STAGE_FRAGMENT,
    },
    {
      .name = "model_ubo",
      .binding = 2,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_VERTEX,
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

qbShaderResourceLayout create_material_resource_layout() {
  qbShaderResourceLayout resource_layout;
  qbShaderResourceBinding_ bindings[] = {
    {
      .name = "material_ubo",
      .binding = 1,
      .resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER,
      .resource_count = 1,
      .stages = QB_SHADER_STAGE_FRAGMENT,
    },
    {
      .name = "albedo_map",
      .binding = 0,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "normal_map",
      .binding = 1,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "metallic_map",
      .binding = 2,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "roughness_map",
      .binding = 3,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "ao_map",
      .binding = 4,
      .resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
      .resource_count = 1,
    },
    {
      .name = "emission_map",
      .binding = 5,
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
    layout (location = 1) in vec3 aNorm;
    layout (location = 2) in vec2 aTexCoords;

    layout(std140, binding = 0) uniform CameraUbo {
        mat4 viewproj;
        vec3 eye;
        float frame;
    } camera_ubo;

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

    const int MAX_POINT_LIGHTS = 1024;
    const int MAX_DIRECTIONAL_LIGHTS = 4;
    const int MAX_SPOT_LIGHTS = 8;

    layout(std140, binding = 1) uniform LightUbo {
      LightPoint point_lights[MAX_POINT_LIGHTS];
      LightDirectional dir_lights[MAX_DIRECTIONAL_LIGHTS];

      // pass_id = 0 --> directional lights
      // pass_id = 1 --> point lights
      int pass_id;
      int num_dir_lights;
    } light_ubo;

    layout(std140, binding = 2) uniform ModelUbo {
        mat4 parent;
        mat4 model;
    } model_ubo;

    out Params
    {
      vec2 TexCoords;
      flat int light_id;
      flat vec3 cam_eye;
    } params;

    void main()
    {
      if (light_ubo.pass_id == 0) {
        gl_Position = vec4(in_pos, 1.f);
      } else if (light_ubo.pass_id == 1) {
        vec3 scale = vec3(light_ubo.point_lights[gl_InstanceID].radius);
        vec3 pos = light_ubo.point_lights[gl_InstanceID].pos;

        mat4 model = mat4(
          vec4(scale.x, 0.0,     0.0,     0.0),
          vec4(0.0,     scale.y, 0.0,     0.0),
          vec4(0.0,     0.0,     scale.z, 0.0),
          vec4(pos.x,   pos.y,   pos.z,   1.0)
        );

        params.light_id = gl_InstanceID;
        params.cam_eye = camera_ubo.eye;

        vec4 world_pos = model * vec4(in_pos, 1.0);
        gl_Position = camera_ubo.viewproj * world_pos;
      }
    })";

  attr.fs = R"(
    #version 330 core

    layout (location = 0) out vec4 out_color;

    in vec2 TexCoords;
    in flat int light_id;
    in flat vec3 cam_eye;

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

    const int MAX_POINT_LIGHTS = 1024;
    const int MAX_DIRECTIONAL_LIGHTS = 4;
    const int MAX_SPOT_LIGHTS = 8;

    layout(std140, binding = 1) uniform LightUbo {
      LightPoint point_lights[MAX_POINT_LIGHTS];
      LightDirectional dir_lights[MAX_DIRECTIONAL_LIGHTS];

      // pass_id = 0 --> directional lights
      // pass_id = 1 --> point lights
      int pass_id;
      int num_dir_lights;
    } light_ubo;

    in Params
    {
      vec2 TexCoords;
      flat int light_id;
      flat vec3 cam_eye;
    } params;

    void main()
    {   
        vec2 tex_coords = vec2(gl_FragCoord.x / 1200.f, gl_FragCoord.y / 800.f);

        // retrieve data from gbuffer
        vec3 FragPos = texture(gPosition, tex_coords).rgb;
        vec3 Normal = texture(gNormal, tex_coords).rgb;
        vec3 Diffuse = texture(gAlbedoSpec, tex_coords).rgb;

        if (Normal == vec3(0.f, 0.f, 0.f)) {
          discard;
        }

        float Specular = texture(gAlbedoSpec, tex_coords).a;
    
        // then calculate lighting as usual
        vec3 lighting  = vec3(0.f);//Diffuse * 0.1; // hard-coded ambient component
        vec3 viewDir  = normalize(params.cam_eye - FragPos);

        if (light_ubo.pass_id == 0) {
          for (int i = 0; i < light_ubo.num_dir_lights; ++i) {            
            lighting += Diffuse * light_ubo.dir_lights[i].col * max(dot(Normal, -light_ubo.dir_lights[i].dir), 0.0) * light_ubo.dir_lights[i].brightness;
          }
          out_color = vec4(lighting, 1.f);
        } else if (light_ubo.pass_id == 1) {
          // calculate distance between light source and current fragment
          float distance = length(light_ubo.point_lights[params.light_id].pos - FragPos);

          // diffuse
          vec3 lightDir = normalize(light_ubo.point_lights[params.light_id].pos - FragPos);
          vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light_ubo.point_lights[params.light_id].col;
          // specular
          vec3 halfwayDir = normalize(lightDir + viewDir);  
          float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
          vec3 specular = light_ubo.point_lights[params.light_id].col * spec * Specular;
          // attenuation
          float attenuation = 1.0 / (1.0 + light_ubo.point_lights[params.light_id].linear * distance + light_ubo.point_lights[params.light_id].quadratic * distance * distance);
          diffuse *= attenuation;
          specular *= attenuation;
          lighting += (diffuse + specular);

          out_color = vec4(lighting, 1.0);
        }
    })";

  attr.interpret_as_strings = QB_TRUE;

  qb_shadermodule_create(&shader, &attr);

  return shader;
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
  qbShaderResourcePipelineLayout resource_pipeline_layout,
  qbColorBlendState blend_state,
  qbDepthStencilState depth_stencil_state) {

  qbRenderPipeline pipeline;
  {
    qbRasterizationInfo_ raster_info{
      .raster_mode = QB_POLYGON_MODE_FILL,
      .raster_face = QB_FACE_FRONT_AND_BACK,
      .front_face = QB_FRONT_FACE_CCW,
      .cull_face = QB_FACE_BACK,
      .enable_depth_clamp = QB_FALSE,
      .depth_stencil_state = depth_stencil_state
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
      .blend_state = blend_state,
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
  r->dynamic_cmd_buf_allocator = qb_memallocator_linear(1 << 20);
  r->dynamic_cmd_bufs = std::vector<qbDrawCommandBuffer>(swapchain_size, nullptr);
  {
    qbDrawCommandBufferAttr_ attr{
      .count = 2,
      .allocator = r->dynamic_cmd_buf_allocator,
    };

    qb_drawcmd_create(r->dynamic_cmd_bufs.data(), &attr);
  }

  r->lighting_cmd_bufs = std::vector<qbDrawCommandBuffer>(swapchain_size, nullptr);
  r->lighting_cmd_buf_allocator = qb_memallocator_linear(1 << 20);
  {
    qbDrawCommandBufferAttr_ attr{
      .count = 2,
      .allocator = r->lighting_cmd_buf_allocator,
    };

    qb_drawcmd_create(r->lighting_cmd_bufs.data(), &attr);
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
      qb_shaderresourceset_writeuniform(r->resource_sets[i], 1, r->light_ubo);
      qb_shaderresourceset_writeuniform(r->resource_sets[i], 2, r->model_ubo);
      for (size_t j = 0; j < r->gbuffer_samplers.size(); ++j) {
        qb_shaderresourceset_writeimage(r->resource_sets[i], j, r->gbuffers[j], r->gbuffer_samplers[j]);
      }
    }
  }
}


struct qbRenderer_* qb_defaultrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args) {
  auto resource_layout = create_resource_layout();
  auto pipeline_layout = create_resource_pipeline_layout(resource_layout);
  auto shader_module = create_shader_module();
  auto render_pass = create_renderpass();

  {
    qbDepthStencilState_ depth_stencil_state{
      .depth_test_enable = QB_TRUE,
      .depth_write_enable = QB_FALSE,
      .depth_compare_op = QB_RENDER_TEST_FUNC_LESS,
      .stencil_test_enable = QB_FALSE,
    };

    qbColorBlendState_ blend_state{
      .blend_enable = QB_TRUE,
      .rgb_blend = {
        .op = QB_BLEND_EQUATION_ADD,
        .src = QB_BLEND_FACTOR_ONE,
        .dst = QB_BLEND_FACTOR_ONE,
      },
      .alpha_blend = {
        .op = QB_BLEND_EQUATION_ADD,
        .src = QB_BLEND_FACTOR_SRC_ALPHA,
        .dst = QB_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      },
    };
    ret->render_pipeline = create_render_pipeline(
      ret->shader_module, ret->geometry, ret->render_pass, ret->pipeline_layout, &blend_state, &depth_stencil_state);
  }

  create_resourcesets(ret);

  return (qbRenderer)ret;
}

void qb_defaultrenderer_destroy(qbRenderer renderer) {
  qbDefaultRenderer r = (qbDefaultRenderer)renderer;
  delete (qbDefaultRenderer)renderer;
}
#endif