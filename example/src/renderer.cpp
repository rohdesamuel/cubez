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
        vec3 eye;
        float frame;
    } camera_ubo;

    layout(std140, binding = 2) uniform ModelUbo {
        mat4 parent;
        mat4 model;
    } model_ubo;
    
    out VertexData
    {
      vec3 pos;
      vec3 norm;
      vec2 tex;
    } o;

    void main() {
      mat4 world_mat = model_ubo.parent * model_ubo.model;
      vec4 world_pos = world_mat * vec4(in_pos, 1.0);
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

    uniform sampler2D albedo_map;
    uniform sampler2D normal_map;
    uniform sampler2D metallic_map;
    uniform sampler2D roughness_map;
    uniform sampler2D ao_map;
    uniform sampler2D emission_map;

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

      g_albedospec = vec4(material_ubo.albedo * texture(albedo_map, o.tex).rgb, material_ubo.metallic); //vec4(material_ubo.albedo, material_ubo.metallic);
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

  r->deferred_samplers = std::vector<qbImageSampler>(r->default_textures.size(), nullptr);
  for (size_t i = 0; i < r->deferred_samplers.size(); ++i) {
    qbImageSamplerAttr_ attr{};
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;
    attr.min_filter = QB_FILTER_TYPE_NEAREST;
    qb_imagesampler_create(&r->deferred_samplers[i], &attr);
  }

  r->deferred_fbos = std::vector<qbFrameBuffer>(swapchain_image_count, nullptr);
  for (size_t i = 0; i < swapchain_image_count; ++i) {
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
      qb_shaderresourceset_writeuniform(r->deferred_resource_sets[i], 2, r->model_ubo);

      for (size_t j = 0; j < r->deferred_samplers.size(); ++j) {
        qb_shaderresourceset_writeimage(r->deferred_resource_sets[i], j, r->default_textures[j], r->deferred_samplers[j]);
      }
    }
  }
}

void create_material_resourceset(qbDefaultRenderer r, qbMaterial material) {
  if (r->material_resource_sets.contains(material)) {
    return;
  }

  qbShaderResourceSetAttr_ attr{
    .create_count = (uint32_t)1,
    .layout = r->material_resource_layout
  };
  qbShaderResourceSet resource_set;
  qb_shaderresourceset_create(&resource_set, &attr);

  std::vector<qbImage> textures;
  textures.push_back(material->albedo_map ? material->albedo_map : white_tex);
  textures.push_back(material->normal_map ? material->normal_map : zero_tex);
  textures.push_back(material->metallic_map ? material->metallic_map : zero_tex);
  textures.push_back(material->roughness_map ? material->roughness_map : zero_tex);
  textures.push_back(material->ao_map ? material->ao_map : zero_tex);
  textures.push_back(material->emission_map ? material->emission_map : zero_tex);

  assert(textures.size() == r->deferred_samplers.size());

  for (size_t j = 0; j < r->deferred_samplers.size(); ++j) {
    qb_shaderresourceset_writeimage(resource_set, j, textures[j], r->deferred_samplers[j]);
  }

  qb_shaderresourceset_writeuniform(resource_set, 1, r->material_ubo);
  r->material_resource_sets[material] = resource_set;
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
      {.pos = {0.0f, 0.0f, 0.f}, .norm = {0.f, 0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {1.0f, 1.0f, 0.f}, .norm = {0.f, 0.f, -1.f}, .tex = {1.f, 1.f}},
      {.pos = {1.0f, 0.0f, 0.f}, .norm = {0.f, 0.f, -1.f}, .tex = {1.f, 0.f}},
      {.pos = {0.0f, 0.0f, 0.f}, .norm = {0.f, 0.f, -1.f}, .tex = {0.f, 0.f}},
      {.pos = {0.0f, 1.0f, 0.f}, .norm = {0.f, 0.f, -1.f}, .tex = {0.f, 1.f}},
      {.pos = {1.0f, 1.0f, 0.f}, .norm = {0.f, 0.f, -1.f}, .tex = {1.f, 1.f}},
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
      {.pos = { 1.0f, -1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 1.f}},
      {.pos = {-1.0f, -1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 0.f}},
      {.pos = { 1.0f,  1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {1.f, 1.f}},
      {.pos = {-1.0f,  1.0f,  0.f}, .norm = {0.f, 0.f, 1.f}, .tex = {0.f, 1.f}},
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
        .norm = {0.f, 0.f, -1.f},
        .tex = {}
      });

      vertex_data.push_back({
        .pos = {center.x - x_next, center.y - y_next, 0.f},
        .norm = {0.f, 0.f, -1.f},
        .tex = {}
      });

      vertex_data.push_back({
        .pos = {center.x - x, center.y - y, 0.f},
        .norm = {0.f, 0.f, -1.f},
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
          sphere_vertices.push_back(Vertex{
            .pos = p2,
            .norm = glms_vec3_scale(p2, 1.0f / radius),
            .tex = t2
          });

          sphere_vertices.push_back(Vertex{
            .pos = p1,
            .norm = glms_vec3_scale(p1, 1.0f / radius),
            .tex = t1
          });

          sphere_vertices.push_back(Vertex{
            .pos = p0,
            .norm = glms_vec3_scale(p0, 1.0f / radius),
            .tex = t0
          });
        }

        sphere_vertices.push_back(Vertex{
          .pos = p2,
          .norm = glms_vec3_scale(p2, 1.0f / radius),
          .tex = t2
        });

        sphere_vertices.push_back(Vertex{
          .pos = p0,
          .norm = glms_vec3_scale(p0, 1.0f / radius),
          .tex = t0
        });

        sphere_vertices.push_back(Vertex{
          .pos = p3,
          .norm = glms_vec3_scale(p3, 1.0f / radius),
          .tex = t3
        });
      }
    }


    qbGpuBufferAttr_ attr{
      .data = sphere_vertices.data(),
      .size = sphere_vertices.size() * sizeof(Vertex),
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    qb_gpubuffer_create(&sphere_verts, &attr);
    sphere_verts_count = sphere_vertices.size();
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

void update_camera_ubo(qbDefaultRenderer self, const qbCamera_* camera, uint32_t frame) {
  qb_gpubuffer_update(self->camera_ubo, 0, sizeof(CameraUbo), &self->camera_data[frame]);
}

void update_directional_lights_ubo(qbDefaultRenderer self) {
  LightUbo* l = self->light_ubo_buf;
  int enabled_index = 0;
  for (qbId enabled : self->enabled_directional_lights) {
    l->dir_lights[enabled_index] = self->directional_lights[enabled];
    ++enabled_index;
  }
  l->num_dir_lights = enabled_index;

  qb_gpubuffer_update(self->light_ubo, offsetof(LightUbo, num_dir_lights), sizeof(int32_t), &l->num_dir_lights);
  qb_gpubuffer_update(self->light_ubo, offsetof(LightUbo, dir_lights), sizeof(LightDirectional) * enabled_index, &l->dir_lights);
}

void update_global_ubos(qbDefaultRenderer self, const qbCamera_* camera, uint32_t frame) {
  update_camera_ubo(self, camera, frame);
  update_directional_lights_ubo(self);
}

// TODO: consider using frustum culling to disregard spheres with influence not
// seen by camera.
int update_point_lights_ubo(qbDrawCommandBuffer draw_cmds, qbDefaultRenderer r, const struct qbCamera_* camera, qbFace cull_face) {
  LightUbo* l = r->light_ubo_buf;

  int enabled_index = 0;
  for (qbId enabled : r->enabled_point_lights) {
    float distance = glms_vec3_distance(camera->eye, r->point_lights[enabled].pos);
    float radius = r->point_lights[enabled].radius;

    if (distance <= radius && cull_face == QB_FACE_FRONT) {
      l->point_lights[enabled_index] = r->point_lights[enabled];
      ++enabled_index;
    }

    if (distance > radius && cull_face == QB_FACE_BACK) {
      l->point_lights[enabled_index] = r->point_lights[enabled];
      ++enabled_index;
    }
  }

  qb_drawcmd_pushbuffer(draw_cmds, r->light_ubo, offsetof(LightUbo, point_lights), sizeof(LightPoint) * enabled_index, &l->point_lights);

  return enabled_index;
}

void record_lighting_commands(qbDefaultRenderer self, const qbCamera_* camera, uint32_t frame) {
  qbDrawCommandBuffer draw_cmds = self->lighting_cmd_bufs[frame];

  qbClearValue_ clear_values[2] = {};
  clear_values[0].color = { 0.f, 0.f, 0.f, 1.f };
  clear_values[1].depth = 1.f;

  qbBeginRenderPassInfo_ begin_info{
    .render_pass = self->render_pass,
    .framebuffer = self->fbos[frame],
    .clear_values = clear_values,
    .clear_values_count = 2,
  };

  qb_drawcmd_beginpass(draw_cmds, &begin_info);
  qb_drawcmd_bindpipeline(draw_cmds, self->render_pipeline, QB_TRUE);
  qb_drawcmd_bindshaderresourceset(draw_cmds, self->pipeline_layout, self->resource_sets[frame]);


  qb_drawcmd_setcull(draw_cmds, QB_FACE_BACK);

  // Set pass to directional lights.
  {
    int32_t pass_id = 0;
    qb_drawcmd_pushbuffer(draw_cmds, self->light_ubo, offsetof(LightUbo, pass_id), sizeof(LightUbo::pass_id), &pass_id);
  }

  qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &screenquad_verts);
  qb_drawcmd_draw(draw_cmds, 6, 1, 0, 0);

  // Set pass to point lights.
  {
    int32_t pass_id = 1;
    qb_drawcmd_pushbuffer(draw_cmds, self->light_ubo, offsetof(LightUbo, pass_id), sizeof(LightUbo::pass_id), &pass_id);
  }

  // Draw front-lit (back-face culled) objects.
  int num_enabled = 0;
  qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &sphere_verts);
  num_enabled = update_point_lights_ubo(draw_cmds, self, camera, QB_FACE_BACK);
  qb_drawcmd_draw(draw_cmds, sphere_verts_count, num_enabled, 0, 0);

  // Draw back-lit (front-face culled) objects.
  qb_drawcmd_setcull(draw_cmds, QB_FACE_FRONT);
  num_enabled = update_point_lights_ubo(draw_cmds, self, camera, QB_FACE_FRONT);
  qb_drawcmd_draw(draw_cmds, sphere_verts_count, num_enabled, 0, 0);

  qb_drawcmd_endpass(draw_cmds);
}

void record_command_buffers(qbDefaultRenderer self, qbDrawCommandBuffer draw_cmds, DrawCommandQueue& command_queue, uint32_t frame) {  
  for (auto& [material, cmds] : command_queue.commands) {
    if (material) {
      // Bind MaterialUbo
      // Bind material samplers
      qb_drawcmd_bindshaderresourceset(draw_cmds, self->deferred_pipeline_layout, self->material_resource_sets[material]);
      qb_drawcmd_updatebuffer(draw_cmds, self->material_ubo, 0, sizeof(MaterialUbo), material);        
    }    

    for (auto& draw_cmd : cmds) {
      if (!material) {
        qbMaterial_ default_mat{};
        default_mat.albedo = { draw_cmd.args.color.x, draw_cmd.args.color.y, draw_cmd.args.color.z };
        qb_drawcmd_pushbuffer(draw_cmds, self->material_ubo, 0, sizeof(MaterialUbo), &default_mat);
      }

      mat4s model_mat = draw_cmd.args.transform;
      switch (draw_cmd.type) {
        case QB_DRAW_TRI:
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &tri_verts);
          qb_drawcmd_draw(draw_cmds, 3, 1, 0, 0);
          break;

        case QB_DRAW_QUAD:
          qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, model), sizeof(mat4s), &model_mat);
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &quad_verts);
          qb_drawcmd_draw(draw_cmds, 6, 1, 0, 0);
          break;

        case QB_DRAW_CIRCLE:
          qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, model), sizeof(mat4s), &model_mat);
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &circle_verts);
          qb_drawcmd_draw(draw_cmds, circle_verts_count, 1, 0, 0);
          break;

        case QB_DRAW_BOX:
          qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, model), sizeof(mat4s), &model_mat);
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &box_verts);
          qb_drawcmd_draw(draw_cmds, box_verts_count, 1, 0, 0);
          break;

        case QB_DRAW_SPHERE:
          qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, model), sizeof(mat4s), &model_mat);
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &sphere_verts);
          qb_drawcmd_draw(draw_cmds, sphere_verts_count, 1, 0, 0);
          break;

        case QB_DRAW_MESH:
          qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, model), sizeof(mat4s), &model_mat);
          qb_drawcmd_bindvertexbuffers(draw_cmds, 0, 1, &draw_cmd.command.mesh.mesh->vbo);

          if (draw_cmd.command.mesh.mesh->ibo) {
            qb_drawcmd_bindindexbuffer(draw_cmds, draw_cmd.command.mesh.mesh->ibo);
            qb_drawcmd_drawindexed(draw_cmds, draw_cmd.command.mesh.mesh->index_count, 1, 0, 0);
          }
          else {
            qb_drawcmd_draw(draw_cmds, draw_cmd.command.mesh.mesh->vertex_count, 1, 0, 0);
          }
          break;

        case QB_DRAW_SPRITE:
          break;
      }
    }
  }
}

void record_all_command_buffers(qbDefaultRenderer self, qbDrawCommandBuffer draw_cmds, DrawCommandQueue& command_queue, uint32_t frame) {
  qbClearValue_ clear_values[4] = {};
  clear_values[0].color = { 0.f, 0.f, 0.f, 0.f };  // Position
  clear_values[1].color = { 0.f, 0.f, 0.f, 0.f };  // Normal
  clear_values[2].color = self->clear_value.color;  // Albedo-specular
  clear_values[3].depth = 1.f;

  qbBeginRenderPassInfo_ begin_info{
    .render_pass = self->deferred_render_pass,
    .framebuffer = self->deferred_fbos[frame],
    .clear_values = clear_values,
    .clear_values_count = 4,
  };

  qb_drawcmd_beginpass(draw_cmds, &begin_info);
  qb_drawcmd_bindpipeline(draw_cmds, self->deferred_render_pipeline, QB_TRUE);  

  // Bind CameraUbo
  // Bind ModelUbo
  qb_drawcmd_bindshaderresourceset(draw_cmds, self->deferred_pipeline_layout, self->deferred_resource_sets[frame]);

  mat4s parent_mat = GLMS_MAT4_IDENTITY_INIT;
  qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, parent), sizeof(mat4s), &parent_mat);
  record_command_buffers(self, draw_cmds, command_queue, frame);

  for (const qbDrawCommand_& cmd: self->static_cmd_bufs) {
    qbCommandBatch batch = cmd.command.batch.batch;

    if (qb_commandbatch_isdynamic(batch)) {
      parent_mat = cmd.args.transform;
      qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, parent), sizeof(mat4s), &parent_mat);
    }

    qb_drawcmd_subcommands(draw_cmds, qb_commandbatch_cmds(batch, frame));

    if (qb_commandbatch_isdynamic(batch)) {
      parent_mat = GLMS_MAT4_IDENTITY_INIT;
      qb_drawcmd_pushbuffer(draw_cmds, self->model_ubo, offsetof(ModelUbo, parent), sizeof(mat4s), &parent_mat);
    }
  }

  qb_drawcmd_endpass(draw_cmds);
}

void record_static_command_buffers(qbDefaultRenderer self, qbDrawCommandBuffer draw_cmds, DrawCommandQueue& command_queue, uint32_t frame) {
  qbBeginRenderPassInfo_ begin_info{
    .render_pass = self->deferred_render_pass,
    .framebuffer = self->deferred_fbos[frame],
    .clear_values = nullptr,
    .clear_values_count = 0,
  };

  qb_drawcmd_beginpass(draw_cmds, &begin_info);
  qb_drawcmd_bindpipeline(draw_cmds, self->deferred_render_pipeline, QB_FALSE);

  record_command_buffers(self, draw_cmds, command_queue, frame);

  qb_drawcmd_endpass(draw_cmds);
}

void render(struct qbRenderer_* self, qbRenderEvent event) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;


  uint32_t frame = qb_swapchain_waitforframe(r->swapchain);  

  const qbCamera_* camera = r->camera;
  if (camera) {
    r->camera_data[frame].viewproj = glms_mat4_mul(camera->projection_mat, camera->view_mat);
    r->camera_data[frame].eye = camera->eye;
    r->camera_data[frame].frame = frame;

    update_global_ubos(r, camera, frame);
    record_all_command_buffers(r, r->dynamic_cmd_bufs[frame], r->draw_commands, frame);
    record_lighting_commands(r, camera, frame);

    qbDrawCommandSubmitInfo_ submit_info{};

    qb_drawcmd_submit(r->dynamic_cmd_bufs[frame], &submit_info);
    qb_drawcmd_submit(r->lighting_cmd_bufs[frame], &submit_info);
    qb_drawcmd_clear(r->dynamic_cmd_bufs[frame]);
    qb_drawcmd_clear(r->lighting_cmd_bufs[frame]);
  }
  
  qbDrawPresentInfo_ present_info{
    .image_index = frame
  };
  qb_swapchain_present(r->swapchain, &present_info);
  qb_swapchain_swap(r->swapchain);

  // Set the size of the commands to 0 to reuse the map between frames.
  for (auto& c : r->draw_commands.commands) {
    c.second.resize(0);
  }
  r->static_cmd_bufs.resize(0);
  r->camera = nullptr;
}

qbResult drawcommands_submit(struct qbRenderer_* self, size_t cmd_count, struct qbDrawCommand_* cmds) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  for (size_t i = 0; i < cmd_count; ++i) {
    if (cmds[i].args.material) {
      create_material_resourceset(((qbDefaultRenderer)self), cmds[i].args.material);
    }

    if (cmds[i].type == QB_DRAW_BATCH) {
      r->static_cmd_bufs.push_back(std::move(cmds[i]));
    } else {
      r->draw_commands.push(std::move(cmds[i]));
    }
  }

  return QB_OK;
}

qbDrawCommandBuffer* drawcommands_compile(qbRenderer self, size_t cmd_count, qbDrawCommand cmds, qbDrawCompileAttr attr, uint32_t* count) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;

  DrawCommandQueue queue{};
  for (size_t i = 0; i < cmd_count; ++i) {
    if (cmds[i].args.material) {
      create_material_resourceset(r, cmds[i].args.material);
    }
    queue.push(std::move(cmds[i]));
  }

  qbDrawCommandBuffer* buf = new qbDrawCommandBuffer[2];
  {
    qbDrawCommandBufferAttr_ attr{
      .count = r->swapchain_images.size(),
      .allocator = qb_memallocator_paged(4096),
    };
    qb_drawcmd_create(buf, &attr);
  }

  for (uint32_t frame = 0; frame < r->swapchain_images.size(); ++frame) {
    record_static_command_buffers(r, buf[frame], queue, frame);
  }

  *count = r->swapchain_images.size();
  return buf;
}

void light_enable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < MAX_DIRECTIONAL_LIGHTS) {
      r->enabled_directional_lights.insert(id);
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < MAX_POINT_LIGHTS) {
      r->enabled_point_lights.insert(id);
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < MAX_SPOT_LIGHTS) {
      r->enabled_spot_lights.insert(id);
    }
    break;
  }
}

void light_disable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < MAX_DIRECTIONAL_LIGHTS) {
      r->enabled_directional_lights.erase(id);
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < MAX_POINT_LIGHTS) {
      r->enabled_point_lights.erase(id);
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < MAX_SPOT_LIGHTS) {
      r->enabled_spot_lights.erase(id);
    }
    break;
  }
}

bool light_isenabled(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  switch (light_type) {
  case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
    if (id < MAX_DIRECTIONAL_LIGHTS) {
      return r->enabled_directional_lights.contains(id);
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_POINT:
    if (id < MAX_POINT_LIGHTS) {
      return r->enabled_point_lights.contains(id);
    }
    break;
  case qbLightType::QB_LIGHT_TYPE_SPOTLIGHT:
    if (id < MAX_SPOT_LIGHTS) {
      return r->enabled_spot_lights.contains(id);
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

void mesh_create(struct qbRenderer_* self, struct qbMesh_* mesh) {
  {
    qbGpuBufferAttr_ attr{
      .data = nullptr,
      .size = sizeof(Vertex) * mesh->vertex_count,
      .elem_size = sizeof(float),
      .buffer_type = QB_GPU_BUFFER_TYPE_VERTEX,
    };

    std::vector<Vertex> vertices;
    vertices.reserve(mesh->vertex_count);
    
    if (!mesh->normals && !mesh->uvs) {
      for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
        vertices.push_back(Vertex{
          .pos = mesh->vertices[i],
        });
      }
    } else if (!mesh->normals && mesh->uvs) {
      for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
        vertices.push_back(Vertex{
          .pos = mesh->vertices[i],
          .tex = mesh->uvs[i]
          });
      }
    }
    else if (mesh->normals && !mesh->uvs) {
      for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
        vertices.push_back(Vertex{
          .pos = mesh->vertices[i],
          .norm = mesh->normals[i],
          });
      }
    }
    else if (mesh->normals && mesh->uvs) {
      for (uint32_t i = 0; i < mesh->vertex_count; ++i) {
        vertices.push_back(Vertex{
          .pos = mesh->vertices[i],
          .norm = mesh->normals[i],
          .tex = mesh->uvs[i]
          });
      }
    }

    attr.data = vertices.data();

    qb_gpubuffer_create(&mesh->vbo, &attr);
  }

  if (mesh->indices) {
    qbGpuBufferAttr_ attr{
      .data = mesh->indices,
      .size = sizeof(uint32_t) * mesh->index_count,
      .elem_size = sizeof(uint32_t),
      .buffer_type = QB_GPU_BUFFER_TYPE_INDEX,
    };

    qb_gpubuffer_create(&mesh->ibo, &attr);
  }
}

void mesh_destroy(struct qbRenderer_* self, struct qbMesh_* mesh) {
  if (mesh->ibo) {
    qb_gpubuffer_destroy(&mesh->ibo);
  }

  if (mesh->vbo) {
    qb_gpubuffer_destroy(&mesh->vbo);
  }
}

void create_default_textures(qbDefaultRenderer r) {
  qbImageAttr_ attr{};
  attr.type = QB_IMAGE_TYPE_2D;

  {
    char pixel[4] = { (char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF };
    qb_image_raw(&white_tex, &attr, QB_PIXEL_FORMAT_RGBA8, 1, 1, pixel);
  }
  {
    char pixel[4] = { (char)0x00, (char)0x00, (char)0x00, (char)0xFF };
    qb_image_raw(&black_tex, &attr, QB_PIXEL_FORMAT_RGBA8, 1, 1, pixel);
  }
  {
    char pixel[4] = { (char)0x00 };
    qb_image_raw(&zero_tex, &attr, QB_PIXEL_FORMAT_RGBA8, 1, 1, pixel);
  }

  r->default_textures = {
    white_tex, /* albedo_map */
    zero_tex, /* normal_map */
    zero_tex, /* metallic_map */
    zero_tex, /* roughness_map */
    zero_tex, /* ao_map */
    zero_tex, /* emission_map */
  };
}

qbResult draw_beginframe(struct qbRenderer_* self, const struct qbCamera_* camera, qbClearValue clear) {
  qbDefaultRenderer r = (qbDefaultRenderer)self;
  r->camera = camera;
  r->clear_value = *clear;
  return QB_OK;
}

struct qbRenderer_* qb_defaultrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args) {
  qbDefaultRenderer ret = new qbDefaultRenderer_{};
  ret->renderer.draw_beginframe = draw_beginframe;
  ret->renderer.drawcommands_submit = drawcommands_submit;
  ret->renderer.drawcommands_compile = drawcommands_compile;
  ret->renderer.render = render;
  ret->renderer.light_enable = light_enable;
  ret->renderer.light_disable = light_disable;
  ret->renderer.light_isenabled = light_isenabled;
  ret->renderer.light_directional = light_directional;
  ret->renderer.light_point = light_point;
  ret->renderer.light_spot = light_spot;
  ret->renderer.light_max = light_max;
  ret->renderer.mesh_create = mesh_create;

  ret->geometry = create_geometry_descriptor();
  ret->camera_ubo = create_uniform<CameraUbo>();
  ret->model_ubo = create_uniform<ModelUbo>();
  ret->material_ubo = create_uniform<MaterialUbo>();
  ret->light_ubo = create_uniform<LightUbo>();
  ret->light_ubo_buf = new LightUbo{};

  ret->resource_layout = create_resource_layout();
  ret->pipeline_layout = create_resource_pipeline_layout(ret->resource_layout);
  ret->shader_module = create_shader_module();
  ret->render_pass = create_renderpass();

  ret->deferred_resource_layout = create_deferred_resource_layout();
  ret->deferred_pipeline_layout = create_resource_pipeline_layout(ret->deferred_resource_layout);
  ret->deferred_shader_module = create_deferred_shader_module();
  ret->deferred_render_pass = create_deferred_renderpass();

  ret->material_resource_layout = create_material_resource_layout();

  create_default_textures(ret);
  create_swapchain(width, height, ret->render_pass, ret);

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

  {
    qbDepthStencilState_ depth_stencil_state{
      .depth_test_enable = QB_TRUE,
      .depth_write_enable = QB_TRUE,
      .depth_compare_op = QB_RENDER_TEST_FUNC_LESS,
      .stencil_test_enable = QB_FALSE,
    };

    qbColorBlendState_ blend_state{};
    ret->deferred_render_pipeline = create_render_pipeline(
      ret->deferred_shader_module, ret->geometry, ret->deferred_render_pass, ret->deferred_pipeline_layout, &blend_state, &depth_stencil_state);
  }

  create_vbos(ret);
  create_resourcesets(ret);
  create_deferred_resourcesets(ret);
  
  return (qbRenderer)ret;
}

void qb_defaultrenderer_destroy(qbRenderer renderer) {
  qbDefaultRenderer r = (qbDefaultRenderer)renderer;
  delete (qbDefaultRenderer)renderer;
}
