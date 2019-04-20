#include <cubez/cubez.h>
#include <cubez/utils.h>

#include "ball.h"
#include "physics.h"
#include "player.h"
#include "log.h"
#include "mesh.h"
#include "mesh_builder.h"
#include "input.h"
#include "render.h"
#include "shader.h"
#include "planet.h"
#include "gui.h"
#include "pong.h"

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <glm/ext.hpp>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

qbRenderModule renderer;
qbRenderPipeline render_pipeline;
qbFrameBuffer window_frame_buffer;

struct Matrices {
  alignas(16) glm::mat4 mvp;
};

qbRenderPass scene_renderpass;
qbRenderPass gui_renderpass;
qbGpuBuffer camera_ubo = {};
qbGpuBuffer model_ubo = {};

void initialize_universe(qbUniverse* uni) {
  qb_init(uni);

  {
    logging::initialize();
  }

  {
    //physics::Settings settings;
    //physics::initialize(settings);
  }

  {
    input::initialize();
  }

  {
    Settings s;
    s.title = "Cubez example";
    s.width = 1200;
    s.height = 800;
    s.znear = 0.1f;
    s.zfar = 10000.0f;
    s.fov = 70.0f;
    render_initialize(s);

    qb_camera_setyaw(90.0f);
    qb_camera_setpitch(0.0f);
    qb_camera_setposition({ 400.0f, -250.0f, 250.0f });
  }

  {
    gui::Initialize();
  }

  {
    qb_render_makecurrent();

    uint32_t width = 1200;
    uint32_t height = 800;
    float scale = 1.0f;
    
    qb_renderpipeline_create(&render_pipeline);

    qbRenderPass present_pass;

    {
      qbFrameBufferAttr_ attr = {};
      attr.width = width;
      attr.height = height;
      attr.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
      attr.color_binding = 0;
      qb_framebuffer_create(&window_frame_buffer, &attr);
    }
    CHECK_GL();

    {
      // https://stackoverflow.com/questions/40450342/what-is-the-purpose-of-binding-from-vkvertexinputbindingdescription
      qbBufferBinding_ attribute_bindings[2];
      {
        qbBufferBinding binding = attribute_bindings;
        binding->binding = 0;
        binding->stride = 8 * sizeof(float);
        binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
      }
      {
        qbBufferBinding binding = attribute_bindings + 1;
        binding->binding = 1;
        binding->stride = 2 * sizeof(float);
        binding->input_rate = QB_VERTEX_INPUT_RATE_INSTANCE;
      }

      qbVertexAttribute_ attributes[4] = {};
      {
        qbVertexAttribute_* attr = attributes;
        attr->binding = 0;
        attr->location = 0;

        attr->count = 3;
        attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
        attr->normalized = false;
        attr->offset = (void*)0;
      }
      {
        qbVertexAttribute_* attr = attributes + 1;
        attr->binding = 0;
        attr->location = 1;

        attr->count = 3;
        attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
        attr->normalized = false;
        attr->offset = (void*)(3 * sizeof(float));
      }
      {
        qbVertexAttribute_* attr = attributes + 2;
        attr->binding = 0;
        attr->location = 2;

        attr->count = 2;
        attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
        attr->normalized = false;
        attr->offset = (void*)(6 * sizeof(float));
      }
      {
        qbVertexAttribute_* attr = attributes + 3;
        attr->binding = 1;
        attr->location = 3;

        attr->count = 2;
        attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
        attr->normalized = false;
        attr->offset = (void*)0;
      }

      qbShaderModule shader_module;
      {
        qbShaderResourceInfo_ resources[4];
        {
          qbShaderResourceInfo info = resources;
          info->binding = 0;
          info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
          info->stages = QB_SHADER_STAGE_VERTEX;
          info->name = "Camera";
        }
        {
          qbShaderResourceInfo info = resources + 1;
          info->binding = 1;
          info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
          info->stages = QB_SHADER_STAGE_VERTEX;
          info->name = "Model";
        }
        {
          qbShaderResourceInfo info = resources + 2;
          info->binding = 2;
          info->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
          info->stages = QB_SHADER_STAGE_FRAGMENT;
          info->name = "tex_sampler_1";
        }
        {
          qbShaderResourceInfo info = resources + 3;
          info->binding = 3;
          info->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
          info->stages = QB_SHADER_STAGE_FRAGMENT;
          info->name = "tex_sampler_2";
        }

        qbShaderModuleAttr_ attr = {};
        attr.vs = "resources/simple.vs";
        attr.fs = "resources/simple.fs";
        attr.resources = resources;
        attr.resources_count = sizeof(resources) / sizeof(resources[0]);

        qb_shadermodule_create(&shader_module, &attr);
      }
      {        
        {
          qbGpuBufferAttr_ attr;
          attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
          attr.data = nullptr;
          attr.size = sizeof(Matrices);

          qb_gpubuffer_create(&camera_ubo, &attr);
        }

        uint32_t bindings[] = { 0 };
        qbGpuBuffer ubo_buffers[] = { camera_ubo };
        qb_shadermodule_attachuniforms(shader_module, 1, bindings, ubo_buffers);
      }
      {
        qbImageSampler image_samplers[2];
        {
          qbImageSamplerAttr_ attr = {};
          attr.image_type = QB_IMAGE_TYPE_2D;
          attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
          attr.mag_filter = QB_FILTER_TYPE_LINEAR;
          qb_imagesampler_create(image_samplers, &attr);
        }
        {
          qbImageSamplerAttr_ attr = {};
          attr.image_type = QB_IMAGE_TYPE_2D;
          attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
          attr.mag_filter = QB_FILTER_TYPE_LINEAR;
          qb_imagesampler_create(image_samplers + 1, &attr);
        }

        uint32_t bindings[] = { 2, 3 };
        qb_shadermodule_attachsamplers(shader_module, 2, bindings, image_samplers);
      }
     
      {
        qbRenderPassAttr_ attr = {};
        attr.frame_buffer = window_frame_buffer;

        attr.bindings = attribute_bindings;
        attr.bindings_count = sizeof(attribute_bindings) / sizeof(attribute_bindings[0]);

        attr.attributes = attributes;
        attr.attributes_count = sizeof(attributes) / sizeof(attributes[0]);

        attr.shader = shader_module;

        attr.viewport = { 0.0, 0.0, width, height };
        attr.viewport_scale = scale;

        qbClearValue_ clear;
        clear.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
        clear.color = { 0.0, 0.0, 0.0, 1.0 };
        clear.depth = 1.0f;
        attr.clear = clear;
      
        qb_renderpass_create(&scene_renderpass, &attr);
        qb_renderpipeline_append(render_pipeline, scene_renderpass);
      }
#if 1
      {
        qbGpuBuffer gpu_buffers[3];
        {
          qbGpuBufferAttr_ attr;

          float vertices[] = {
            // Positions         // Colors          // UVs
            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,

            -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,

            -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,

             0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,

            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,

            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
          };

          attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
          attr.data = vertices;
          attr.size = sizeof(vertices);
          attr.elem_size = sizeof(vertices[0]);

          qb_gpubuffer_create(gpu_buffers, &attr);
        }
        {
          qbGpuBufferAttr_ attr;

          glm::vec2 translations[100];
          int index = 0;
          float offset = 0.1f;
          for (int y = -10; y < 10; y += 2) {
            for (int x = -10; x < 10; x += 2) {
              glm::vec2 translation;
              translation.x = (float)x / 1.5f + offset;
              translation.y = (float)y / 1.5f + offset;
              translations[index++] = translation;
            }
          }

          attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
          attr.data = translations;
          attr.size = sizeof(translations);
          attr.elem_size = sizeof(float);

          qb_gpubuffer_create(gpu_buffers + 1, &attr);
        }
        {
          qbGpuBufferAttr_ attr;

          int indices[] = {
            3,  1,  0,  3,  2,  1,
            4,  5,  7,  5,  6,  7,
            8,  9,  11, 9,  10, 11,
            15, 13, 12, 15, 14, 13,
            16, 17, 19, 17, 18, 19,
            23, 21, 20, 23, 22, 21,
          };

          attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
          attr.data = indices;
          attr.size = sizeof(indices);
          attr.elem_size = sizeof(indices[0]);

          qb_gpubuffer_create(gpu_buffers + 2, &attr);
        }

        qbImage ball_image;
        {
          qbImageAttr_ attr;
          attr.type = QB_IMAGE_TYPE_2D;
          qb_image_load(&ball_image, &attr, "resources/ball.bmp");
        }

        qbImage soccer_ball_image;
        {
          qbImageAttr_ attr;
          attr.type = QB_IMAGE_TYPE_2D;
          qb_image_load(&soccer_ball_image, &attr, "resources/soccer_ball.bmp");
        }

        qbMeshBuffer draw_buff;
        {
          qbMeshBufferAttr_ attr = {};
          attr.bindings_count = qb_renderpass_bindings(scene_renderpass, &attr.bindings);
          attr.attributes_count = qb_renderpass_attributes(scene_renderpass, &attr.attributes);
          attr.instance_count = 100;

          qb_meshbuffer_create(&draw_buff, &attr);
          qb_renderpass_append(scene_renderpass, draw_buff);
        }

        qbGpuBuffer vertices[] = { gpu_buffers[0], gpu_buffers[1] };
        qb_meshbuffer_attachvertices(draw_buff, vertices);

        qb_meshbuffer_attachindices(draw_buff, gpu_buffers[2]);

        uint32_t image_bindings[] = { 2, 3 };
        qbImage images[] = { soccer_ball_image, ball_image };
        qb_meshbuffer_attachimages(draw_buff, 2, image_bindings, images);

        {
          {
            qbGpuBufferAttr_ attr;
            attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
            attr.data = nullptr;
            attr.size = sizeof(Matrices);

            qb_gpubuffer_create(&model_ubo, &attr);
          }
          qbGpuBuffer ubo_buffers[] = { model_ubo };

          uint32_t bindings[] = { 1 };
          qbGpuBuffer buffers[] = { ubo_buffers[0] };
          qb_meshbuffer_attachuniforms(draw_buff, 1, bindings, buffers);
        }

      }
#endif
    }
    CHECK_GL();

    gui_renderpass = gui::CreateGuiRenderPass(window_frame_buffer, width, height);
    qb_renderpipeline_append(render_pipeline, gui_renderpass);

    {
      qbVertexAttribute_ attributes[2] = {};
      {
        qbVertexAttribute_* attr = attributes;
        attr->binding = 0;
        attr->location = 0;

        attr->count = 2;
        attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
        attr->normalized = GL_FALSE;
        attr->offset = (void*)0;
      }
      {
        qbVertexAttribute_* attr = attributes + 1;
        attr->binding = 0;
        attr->location = 1;

        attr->count = 2;
        attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
        attr->normalized = GL_FALSE;
        attr->offset = (void*)(2 * sizeof(float));
      }

      qbShaderModule shader_module;
      {
        qbShaderResourceInfo_ sampler_attr = {};
        sampler_attr.binding = 0;
        sampler_attr.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
        sampler_attr.stages = QB_SHADER_STAGE_FRAGMENT;
        sampler_attr.name = "tex_sampler";

        qbShaderModuleAttr_ attr = {};
        attr.vs = "resources/texture.vs";
        attr.fs = "resources/texture.fs";

        attr.resources = &sampler_attr;
        attr.resources_count = 1;

        qb_shadermodule_create(&shader_module, &attr);

        qbImageSampler sampler;
        {
          qbImageSamplerAttr_ attr = {};
          attr.image_type = QB_IMAGE_TYPE_2D;
          attr.min_filter = QB_FILTER_TYPE_LINEAR;
          attr.mag_filter = QB_FILTER_TYPE_LINEAR;
          qb_imagesampler_create(&sampler, &attr);
        }

        uint32_t bindings[] = { 0 };
        qbImageSampler samplers[] = { sampler };
        qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
      }

      qbBufferBinding_ binding;
      binding.binding = 0;
      binding.stride = 4 * sizeof(float);
      binding.input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
      {
        qbRenderPassAttr_ attr = {};
        attr.frame_buffer = nullptr;

        attr.bindings = &binding;
        attr.bindings_count = 1;

        attr.attributes = attributes;
        attr.attributes_count = 2;

        attr.shader = shader_module;

        attr.viewport = { 0.0, 0.0, width, height };
        attr.viewport_scale = scale;

        qbClearValue_ clear;
        clear.attachments = (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT);
        clear.color = { 0.0, 0.0, 0.0, 1.0 };
        attr.clear = clear;

        qb_renderpass_create(&present_pass, &attr);
        qb_renderpipeline_append(render_pipeline, present_pass);
      }

      qbGpuBuffer gpu_buffers[2];
      {
        qbGpuBufferAttr_ attr;
        float vertices[] = {
          // Positions   // UVs
          -1.0, -1.0,    0.0, 0.0,
          -1.0,  1.0,    0.0, 1.0,
           1.0,  1.0,    1.0, 1.0,
           1.0, -1.0,    1.0, 0.0
        };

        attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
        attr.data = vertices;
        attr.size = sizeof(vertices);
        attr.elem_size = sizeof(float);
        qb_gpubuffer_create(gpu_buffers, &attr);
      }
      {
        qbGpuBufferAttr_ attr;
        int indices[] = {
          0, 1, 2,
          0, 2, 3,
        };
        attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
        attr.data = indices;
        attr.size = sizeof(indices);
        attr.elem_size = sizeof(int);
        qb_gpubuffer_create(gpu_buffers + 1, &attr);
      }

      qbMeshBuffer draw_buff;
      {
        qbMeshBufferAttr_ attr = {};
        attr.attributes_count = qb_renderpass_attributes(present_pass, &attr.attributes);
        attr.bindings_count = qb_renderpass_bindings(present_pass, &attr.bindings);

        qb_meshbuffer_create(&draw_buff, &attr);
        qb_renderpass_append(present_pass, draw_buff);
      }

      qbGpuBuffer vertices[] = { gpu_buffers[0] };
      qb_meshbuffer_attachvertices(draw_buff, vertices);
      qb_meshbuffer_attachindices(draw_buff, gpu_buffers[1]);

      uint32_t image_bindings[] = { 0 };
      qbImage images[] = { qb_framebuffer_rendertarget(window_frame_buffer) };
      qb_meshbuffer_attachimages(draw_buff, 1, image_bindings, images);
    }

    CHECK_GL();
  }
  
#if 0
  {
    Settings s;
    s.title = "Cubez example";
    s.width = 1200;
    s.height = 800;
    s.znear = 0.1f;
    s.zfar = 10000.0f;
    s.fov = 70.0f;
    initialize(s);

    qb_camera_setyaw(90.0f);
    qb_camera_setpitch(0.0f);
    qb_camera_setposition({400.0f, -250.0f, 250.0f});

    check_for_gl_errors();
  }


  qbShader mesh_shader;
  qbTexture ball_texture;
  MeshBuilder block_mesh;
  qbMaterial ball_material;
  qbMaterial red_material;
  {
    // Load resources.
    std::cout << "Loading resources.\n";
    qb_shader_load(&mesh_shader, "mesh_shader", "resources/mesh.vs", "resources/mesh.fs");
    qb_texture_load(&ball_texture, "ball_texture", "resources/soccer_ball.bmp");
    block_mesh = MeshBuilder::FromFile("resources/block.obj");

    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setcolor(attr, glm::vec4{ 1.0, 0.0, 1.0, 1.0 });
    qb_materialattr_addtexture(attr, ball_texture, {0, 0}, {0, 0});
    qb_materialattr_setshader(attr, mesh_shader);
    qb_material_create(&ball_material, attr);
    qb_materialattr_destroy(&attr);
  }
  {
    qbMaterialAttr attr;
    qb_materialattr_create(&attr);
    qb_materialattr_setcolor(attr, glm::vec4{ 1.0, 0.0, 0.0, 1.0 });
    qb_materialattr_setshader(attr, mesh_shader);
    qb_material_create(&red_material, attr);
    qb_materialattr_destroy(&attr);
  }

  {
    ball::Settings settings;
    settings.mesh = block_mesh.BuildRenderable(qbRenderMode::QB_TRIANGLES);
    settings.collision_mesh = new Mesh(block_mesh.BuildMesh());
    settings.material = ball_material;
    settings.material_exploded = red_material;

    ball::initialize(settings);
    check_for_gl_errors();
  }
#if 0
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    physics::Transform t{{400.0f, 300.0f, -32.0f}, {}, true};
    qb_entityattr_addcomponent(attr, physics::component(), &t);

    qbMesh mesh;
    qb_mesh_load(&mesh, "floor_mesh", "C:\\Users\\Sam\\Source\\Repos\\cubez\\windows\\cubez\\x64\\Release\\floor.obj");
    create(mesh, ball_material);

    qbEntity unused;
    qb_entity_create(&unused, attr);

    qb_entityattr_destroy(&attr);
  }
#endif
  {
    player::Settings settings;
    settings.start_pos = {0, 0, 0};

    player::initialize(settings);
    check_for_gl_errors();
  }

  auto new_game = [mesh_shader](const framework::JSObject&, const framework::JSArgs&) {
    planet::Settings settings;
    settings.shader = mesh_shader;
    settings.resource_folder = "resources/";
    planet::Initialize(settings);

    return framework::JSValue();
  };
  {
    planet::Settings settings;
    settings.shader = mesh_shader;
    settings.resource_folder = "resources/";
    planet::Initialize(settings);
  }
  {
    glm::vec2 menu_size(window_width(), window_height());
    glm::vec2 menu_pos(0, 0);

    gui::JSCallbackMap callbacks;
    callbacks["NewGame"] = new_game;

    //gui::FromFile("file:///game.html", menu_pos, menu_size, callbacks);
  }

#endif
}

qbScene create_main_menu() {
  qbScene scene;
  qb_scene_create(&scene, "Main Menu");
  qb_scene_set(scene);

  qb_scene_reset();
  return scene;
}

qbCoro test_coro;
qbCoro generator_coro;
qbCoro wait_coro;

qbVar load_texture(qbVar var) {
  qbTexture tex = nullptr;
  qb_texture_load(&tex, "", (const char*)var.p);
  return qbVoid(tex);
}

qbVar generator(qbVar var) {
  qbCoro c = qb_coro_async(load_texture, qbVoid("resources/soccer_ball.bmp"));

  for (;;) {
    var = qb_coro_yield(qbInt(var.i - 1));    

    qbVar ret = qb_coro_await(c);
    if ((ret = qb_coro_peek(c)).tag != QB_TAG_UNSET) {
      logging::out("Texture loaded!");
      var = qbInt(0);
      qb_coro_destroy(&c);
    }
    //qb_coro_wait(1.0);
  }
  return qbNone;
}

qbVar test_entry(qbVar var) {
  qbVar ret = var;
  int num_times_run = 0;

  while (true) {
    if (ret.tag != QB_TAG_UNSET) {
      logging::out("%lld", ret.i);
    }
    ret = qb_coro_call(generator_coro, ret);
    //qb_coro_await(generator_coro);
    num_times_run++;

    qb_coro_yield(qbNone);
    if (ret.tag == QB_TAG_UNSET) {
      continue;
    } else if (ret.i <= 0) {
      break;
    }
  }

  logging::out("number of times run: %d", num_times_run);
  return qbNone;
}

struct TimingInfo {
  uint64_t frame;
  double udpate_fps;
  double render_fps;
  double total_fps;
  double fps_dt1;
  double fps_dt2;
};

qbVar print_timing_info(qbVar var) {
  for (;;) {
    TimingInfo* info = (TimingInfo*)var.p;

    std::string out =
      "Frame: " + std::to_string(info->frame) + "\n"
      "Total FPS:  " + std::to_string(info->total_fps) + "\n"
      "Render FPS: " + std::to_string(info->render_fps) + "\n"
      "Update FPS: " + std::to_string(info->udpate_fps) + "\n";
    //std::cout << (int)info->total_fps << ", " << (int)info->render_fps << ", " << (int)info->udpate_fps << "\n";

    //std::cout << out << std::endl;
    logging::out(out.c_str());

    qb_coro_wait(1.0);
    //qb_coro_waitframes(1);
    //qb_coro_yield(qbNone);
  }
}

qbVar ball_random_walk(qbVar v) {
  qbEntity ball = v.u;

  physics::Transform* t;
  qb_instance_find(physics::component(), ball, &t);

  for (int i = 0; i < 10; ++i) {
    qb_coro_wait(1.0);
    t->v = {
      (((float)(rand() % 1000)) / 1000) - 0.5,
      (((float)(rand() % 1000)) / 1000) - 0.5,
      (((float)(rand() % 1000)) / 1000) - 0.5
    };
  }

  qb_entity_destroy(ball);
  return qbNone;
}

qbVar create_random_balls(qbVar) {
  std::vector<qbCoro> coros;
  coros.reserve(100);
  for (;;) {
    glm::vec3 v = {
      (((float)(rand() % 1000)) / 1000) - 0.5,
      (((float)(rand() % 1000)) / 1000) - 0.5,
      (((float)(rand() % 1000)) / 1000) - 0.5
    };
    qbEntity ball = ball::create({}, v);
    coros.push_back(qb_coro_sync(ball_random_walk, qbUint(ball)));
    qb_coro_wait(1.0);

    for (auto it = coros.begin(); it != coros.end(); ++it) {
      qbCoro c = *it;
      if (qb_coro_done(c)) {
        qb_coro_destroy(&c);
        it = coros.erase(it);
      }
    }
  }
}

qbVar rotate_cube(qbVar vrot) {
  float* rot = (float*)vrot.p;
  for (;;) {
    *rot += 0.1f * 0.2f;
    qb_coro_yield(qbNone);
    qb_coro_wait(0.010);
  }
  return qbNone;
};

struct GridVertex {
  glm::vec3 pos;
  glm::vec3 col;
  glm::vec2 tex;
};

#if 0
qbMeshBuffer CreateGrid(qbRenderPass renderpass) {
  qbGpuBuffer vbo, ebo, ubo;
  std::vector<size_t> indices;
  {
    std::vector<GridVertex> vertices;
    size_t grid_width = 10;
    size_t grid_height = 10;
    size_t cell_width = 32;
    size_t cell_height = 32;
    for (size_t i = 0; i < grid_width; ++i) {
      for (size_t j = 0; j < grid_height; ++j) {
        for (size_t x = 0; x < 2; ++x) {
          for (size_t y = 0; y < 2; ++y) {
            GridVertex v;
            v.pos = { i * grid_width + x * cell_width,
                      j * grid_height + y * cell_height,
                      0.0f };
            v.col = { (float)x, (float)y, (float)(x - y) };
            v.tex = { 0.0f, 0.0f };
            vertices.push_back(v);
          }
        }
        size_t index = vertices.size() - 4;
        indices.push_back(index + 3);
        indices.push_back(index + 1);
        indices.push_back(index + 0);
        indices.push_back(index + 3);
        indices.push_back(index + 2);
        indices.push_back(index + 1);
      }
    }

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = vertices.data();
    attr.size = vertices.size() * sizeof(GridVertex);
    attr.elem_size = sizeof(GridVertex);

    qb_gpubuffer_create(&vbo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = indices.data();
    attr.size = sizeof(indices) * sizeof(size_t);
    attr.elem_size = sizeof(size_t);
    qb_gpubuffer_create(&ebo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(gui::GuiUniformModel);
    qb_gpubuffer_create(&ubo, &attr);
  }

  qbMeshBuffer ret;
  qbMeshBufferAttr_ attr = {};
  attr.attributes_count = qb_renderpass_attributes(renderpass, &attr.attributes);
  attr.bindings_count = qb_renderpass_bindings(renderpass, &attr.bindings);

  qb_meshbuffer_create(&ret, &attr);
  qb_renderpass_appendmeshbuffer(renderpass, ret);

  qbGpuBuffer vertex_buffers[] = { vbo };
  qb_meshbuffer_attachvertices(ret, vertex_buffers);
  qb_meshbuffer_attachindices(ret, ebo);

  uint32_t bindings[] = { gui::GuiUniformModel::Binding() };
  qbGpuBuffer uniform_buffers[] = { ubo };
  qb_meshbuffer_attachuniforms(ret, 1, bindings, uniform_buffers);

  return ret;
}
#endif

TimingInfo timing_info;
int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);
  {
    pong::Settings s;
    s.scene_pass = gui_renderpass;
    s.width = 1200;
    s.height = 800;
    pong::Initialize(s);
    logging::out("Hello world\n");
  }

  //qb_coro_sync(create_random_balls, qbNone);
  //test_coro = qb_coro_sync(test_entry, qbInt(100000));
  //generator_coro = qb_coro_create(generator);

  qb_start();
  
  int frame = 0;

  qbTimer fps_timer;
  qbTimer update_timer;
  qbTimer render_timer;

  qb_coro_sync(print_timing_info, qbVoid(&timing_info));
  qb_coro_sync([](qbVar) {
    while (true) {
      gui::qb_window_updateuniforms();
      qb_coro_wait(0.015);
    }
    return qbNone;
  }, qbNone);

  float* cube_rot = new float;
  *cube_rot = 0.0f;
  qb_coro_sync(rotate_cube, qbVoid(cube_rot));

  gui::qbWindowCallbacks_ window_callbacks;
  window_callbacks.onfocus = [](gui::qbWindow) {
    std::cout << "onfocus\n";
  };
  window_callbacks.onclick = [](gui::qbWindow, input::MouseEvent* e) {
    std::cout << "onclick ( " << e->button_event.mouse_button << ", " << (bool)e->button_event.state << " )\n";
  };
  window_callbacks.onscroll = [](gui::qbWindow window, input::MouseEvent* e) {
    std::cout << "onscroll ( " << e->scroll_event.x << ", " << e->scroll_event.y << " )\n";
    if (input::is_key_pressed(qbKey::QB_KEY_LSHIFT)) {
      gui::qb_window_moveby(window, { e->scroll_event.y * 5, 0.0f, 0.0f });
    } else {
      gui::qb_window_moveby(window, { e->scroll_event.x, e->scroll_event.y * 5, 0.0f });
    }
  };

  qbImage ball_image;
  {
    qbImageAttr_ attr;
    attr.type = QB_IMAGE_TYPE_2D;
    qb_image_load(&ball_image, &attr, "resources/ball.bmp");
  }

  gui::qbWindow parent;
  gui::qb_window_create(&parent, { 1200.0f - 512.0f, 800 - 512.0f, 0.0f }, { 512.0f, 512.0f }, true, &window_callbacks, nullptr, ball_image, {1.0, 1.0, 1.0, 0.75});

  gui::qbWindow child;
  gui::qb_window_create(&child, { 0.0f, -32.0f, 0.0f }, { 32.0f, 32.0f }, true, &window_callbacks, parent, ball_image, { 0.0, 1.0, 0.0, 0.75 });
  gui::qb_window_movetofront(child);

  qb_timer_create(&fps_timer, 50);
  qb_timer_create(&update_timer, 50);
  qb_timer_create(&render_timer, 50);

  const double kClockResolution = 1e9;
  double t = 0.0;
  const double dt = 0.01;
  double current_time = qb_timer_query() * 0.000000001;
  double start_time = (double)qb_timer_query();
  double accumulator = 0.0;
  //gui::qbRenderTarget target;
  //gui::qb_rendertarget_create(&target, { 250, 0 }, { 500, 500 }, {});

  {
    Matrices mat = {};
    mat.mvp = glm::perspective(glm::radians(45.0f), 1200.0f / 800.0f, 0.1f, 100.0f);
    qb_gpubuffer_update(camera_ubo, 0, sizeof(Matrices), &mat);
  }

  while (1) {
    qb_timer_start(fps_timer);

    double new_time = qb_timer_query() * 0.000000001;
    double frame_time = new_time - current_time;
    frame_time = std::min(0.25, frame_time);
    current_time = new_time;

    accumulator += frame_time; //qb_timer_average(render_timer) / 1e9;// 
    
    
    input::handle_input([](SDL_Event*) {
      render_shutdown();
      SDL_Quit();
      qb_stop();
      exit(0);
    });
    
    while (accumulator >= dt) {      
      qb_timer_start(update_timer);
      qb_loop();
      qb_timer_add(update_timer);

      accumulator -= dt;
      t += dt;
    }

    qb_timer_start(render_timer);

    double alpha = accumulator / dt;
    {
      Matrices mat = {};
      mat.mvp = glm::mat4(1.0f);
      glm::mat4 view = glm::mat4(1.0f);
      // note that we're translating the scene in the reverse direction of where we want to move
      view = glm::translate(view, glm::vec3(0.0f, 0.0f, -5.0f));

      glm::mat4 model = glm::mat4(1.0f);
      float prev_rot = (float)(frame - 1) / 50.0f;
      float curr_rot = (float)frame / 50.0f;
      float render_rot = (curr_rot * alpha) + (prev_rot * (1.0f - alpha));
      model = glm::rotate(model, *cube_rot, glm::vec3(1.0f, 0.0f, 0.0f));

      mat.mvp = view * model;// glm::rotate(mat.mvp, (float)frame / 50.0f, glm::vec3(0.0f, 0.0f, 1.0f));
      qb_gpubuffer_update(model_ubo, 0, sizeof(Matrices), &mat);
    }
    
    qbRenderEvent_ e;
    e.frame = frame;
    e.alpha = accumulator / dt;
    
    //pong::OnRender(&e);
    qb_renderpipeline_run(render_pipeline, &e);
    qb_render_swapbuffers();
    qb_timer_add(render_timer);

    ++frame;
    qb_timer_stop(fps_timer);

    // Calculate then communicate TimingInfo to Coroutine.
    auto update_timer_avg = qb_timer_average(update_timer);
    auto render_timer_avg = qb_timer_average(render_timer);
    auto fps_timer_elapsed = qb_timer_elapsed(fps_timer);

    double update_fps = update_timer_avg == 0 ? 0 : kClockResolution / update_timer_avg;
    double render_fps = render_timer_avg == 0 ? 0 : kClockResolution / render_timer_avg;
    double total_fps = fps_timer_elapsed == 0 ? 0 : kClockResolution / fps_timer_elapsed;

    timing_info.frame = frame;
    timing_info.render_fps = render_fps;
    timing_info.total_fps = total_fps;
    timing_info.udpate_fps = update_fps;

    //std::cout << (int)timing_info.total_fps << ", " << (int)timing_info.render_fps << ", " << (int)timing_info.udpate_fps << "\n";
  }
}