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
#include "opengl_render_module.h"

#include <algorithm>
#include <thread>
#include <unordered_map>

const char simple_vs[] = R"(
#version 200

in vec3 inPos;
in vec3 inCol;

out vec3 vCol;

void main() {
  vCol = inCol;
  gl_Position = vec4(inPos, 1.0);
}
)";

const char simple_fs[] = R"(
#version 130

in vec3 vCol;
out vec4 frag_color;

void main() {
  frag_color = vec4(vCol, 1.0);
}
)";

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

void check_for_gl_errors() {
  GLenum error = glGetError();
  if (error) {
    const GLubyte* error_str = gluErrorString(error);
    std::cout << "Error(" << error << "): " << error_str << std::endl;
  }
}

qbRenderModule renderer;
qbRenderPipeline render_pipeline;

struct Matrices {
  alignas(16) glm::mat4 mvp;
};

qbGpuBuffer camera_ubo = {};
qbGpuBuffer model_ubo[2] = {};

void initialize_universe(qbUniverse* uni) {
  qb_init(uni);

  {
    logging::initialize();
  }

  {
    physics::Settings settings;
    physics::initialize(settings);
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
    initialize(s);

    qb_camera_setyaw(90.0f);
    qb_camera_setpitch(0.0f);
    qb_camera_setposition({ 400.0f, -250.0f, 250.0f });

    check_for_gl_errors();
  }

  {
    qb_render_makecurrent();

    uint32_t width = 1200;
    uint32_t height = 800;
    float scale = 1.0f;
    
    renderer = Initialize(width, height, 1.0);

    qb_renderpipeline_create(&render_pipeline, width, height, 1.0);

    qbRenderPass render_pass;
    qbRenderPass gui_pass;
    qbRenderPass present_pass;

    qbFrameBuffer frame;
    {
      qbFrameBufferAttr_ attr;
      attr.width = width;
      attr.height = height;
      qb_framebuffer_create(&frame, &attr);
    }
    CHECK_GL();

    {
      // https://stackoverflow.com/questions/40450342/what-is-the-purpose-of-binding-from-vkvertexinputbindingdescription
      qbBufferBinding_ binding = {};
      binding.binding = 0;
      binding.stride = 8 * sizeof(float);
      binding.input_rate = QB_VERTEX_INPUT_RATE_VERTEX;

      qbVertexAttribute_ attributes[3] = {};
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

      qbShaderModule shader_module;
      {
        qbShaderResourceAttr_ resource_attrs[3];
        {
          qbShaderResourceAttr attr = resource_attrs;
          attr->binding = 0;
          attr->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
          attr->stages = QB_SHADER_STAGE_VERTEX;
          attr->name = "Camera";
        }
        {
          qbShaderResourceAttr attr = resource_attrs + 1;
          attr->binding = 1;
          attr->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
          attr->stages = QB_SHADER_STAGE_VERTEX;
          attr->name = "Model";
        }
        {
          qbShaderResourceAttr attr = resource_attrs + 2;
          attr->binding = 2;
          attr->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
          attr->stages = QB_SHADER_STAGE_VERTEX;
          attr->name = "tex_sampler_1";
        }

        qbShaderModuleAttr_ attr;
        attr.vs = "resources/simple.vs";
        attr.fs = "resources/simple.fs";

        attr.resources = resource_attrs;
        attr.resources_count = sizeof(resource_attrs) / sizeof(resource_attrs[0]);

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
          attr.min_filter = QB_FILTER_TYPE_LINEAR;
          attr.mag_filter = QB_FILTER_TYPE_LINEAR;
          qb_imagesampler_create(image_samplers, &attr);
        }
        {
          qbImageSamplerAttr_ attr = {};
          attr.image_type = QB_IMAGE_TYPE_2D;
          attr.min_filter = QB_FILTER_TYPE_LINEAR;
          attr.mag_filter = QB_FILTER_TYPE_LINEAR;
          qb_imagesampler_create(image_samplers + 1, &attr);
        }

        uint32_t bindings[] = { 2 };
        qbImageSampler* samplers = image_samplers;
        qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
      }
     
      {
        qbRenderPassAttr_ attr;
        attr.frame_buffer = frame;

        attr.bindings = &binding;
        attr.bindings_count = 1; // 1

        attr.attributes = attributes;
        attr.attributes_count = 3; // 2

        attr.shader_program = shader_module;

        attr.viewport = { 0.0, 0.0, width, height };
        attr.viewport_scale = scale;
      
        qb_renderpass_create(&render_pass, &attr, render_pipeline);
      }

      {
        qbGpuBuffer gpu_buffers[2];
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

          int indices[] = {
            3, 1, 0, 3, 2, 1,
            4, 5, 7, 5, 6, 7,
            8, 9, 11,   9, 10, 11,
            15, 13, 12, 15, 14, 13,
            16, 17, 19, 17, 18, 19,
            23, 21, 20, 23, 22, 21,
          };

          attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
          attr.data = indices;
          attr.size = sizeof(indices);
          attr.elem_size = sizeof(indices[0]);

          qb_gpubuffer_create(gpu_buffers + 1, &attr);
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

        qbDrawBuffer draw_buff[2];
        {
          qbDrawBufferAttr_ attr;
          qb_drawbuffer_create(&draw_buff[0], &attr, render_pass);
          qb_drawbuffer_create(&draw_buff[1], &attr, render_pass);
        }


        qbGpuBuffer vertices[] = { gpu_buffers[0] };
        qb_drawbuffer_attachvertices(draw_buff[0], vertices);
        qb_drawbuffer_attachvertices(draw_buff[1], vertices);

        qb_drawbuffer_attachindices(draw_buff[0], gpu_buffers[1]);
        qb_drawbuffer_attachindices(draw_buff[1], gpu_buffers[1]);

        uint32_t image_bindings[] = { 2 };
        qbImage images[] = { soccer_ball_image };
        qb_drawbuffer_attachimages(draw_buff[0], 1, image_bindings, images);
        qb_drawbuffer_attachimages(draw_buff[1], 1, image_bindings, images);

        {
          {
            qbGpuBufferAttr_ attr;
            attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
            attr.data = nullptr;
            attr.size = sizeof(Matrices);

            qb_gpubuffer_create(&model_ubo[0], &attr);
          }
          qbGpuBuffer ubo_buffers[] = { model_ubo[0] };

          uint32_t bindings[] = { 1 };
          qbGpuBuffer buffers[] = { ubo_buffers[0] };
          qb_drawbuffer_attachuniforms(draw_buff[0], 1, bindings, buffers);
        }

        {
          {
            qbGpuBufferAttr_ attr;
            attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
            attr.data = nullptr;
            attr.size = sizeof(Matrices);

            qb_gpubuffer_create(&model_ubo[1], &attr);
          }
          qbGpuBuffer ubo_buffers[] = { model_ubo[1] };

          uint32_t bindings[] = { 1 };
          qbGpuBuffer buffers[] = { ubo_buffers[0] };
          qb_drawbuffer_attachuniforms(draw_buff[1], 1, bindings, buffers);
        }

      }
    }
    CHECK_GL();

    qbRenderPass gui_render_pass = gui::CreateGuiRenderPass(frame, width, height);
    qb_renderpipeline_appendrenderpass(render_pipeline, gui_render_pass);

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
        qbShaderResourceAttr_ sampler_attr;
        sampler_attr.binding = 0;
        sampler_attr.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
        sampler_attr.stages = QB_SHADER_STAGE_FRAGMENT;
        sampler_attr.name = "tex_sampler";

        qbShaderModuleAttr_ attr;
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
        qbRenderPassAttr_ attr;
        attr.frame_buffer = nullptr;

        attr.bindings = &binding;
        attr.bindings_count = 1;

        attr.attributes = attributes;
        attr.attributes_count = 2;

        attr.shader_program = shader_module;

        attr.viewport = { 0.0, 0.0, width, height };
        attr.viewport_scale = scale;

        qb_renderpass_create(&present_pass, &attr, render_pipeline);
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

      qbDrawBuffer draw_buff;
      {
        qbDrawBufferAttr_ attr;
        qb_drawbuffer_create(&draw_buff, &attr, present_pass);
      }

      qbGpuBuffer vertices[] = { gpu_buffers[0] };
      qb_drawbuffer_attachvertices(draw_buff, vertices);
      qb_drawbuffer_attachindices(draw_buff, gpu_buffers[1]);

      uint32_t image_bindings[] = { 0 };
      qbImage images[] = { frame->render_target };
      qb_drawbuffer_attachimages(draw_buff, 1, image_bindings, images);
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
};

qbVar print_timing_info(qbVar var) {
  for (;;) {
    TimingInfo* info = (TimingInfo*)var.p;

    std::string out =
      "Frame: " + std::to_string(info->frame) + "\n"
      "Total FPS:  " + std::to_string(info->total_fps) + "\n"
      "Render FPS: " + std::to_string(info->render_fps) + "\n"
      "Update FPS: " + std::to_string(info->udpate_fps);

    logging::out(out.c_str());

    qb_coro_wait(1.0);
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


TimingInfo timing_info;
int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);

  //qb_coro_sync(create_random_balls, qbNone);
  //test_coro = qb_coro_sync(test_entry, qbInt(100000));
  //generator_coro = qb_coro_create(generator);

  qb_start();
  
  int frame = 0;
  qbTimer fps_timer;
  qbTimer update_timer;
  qbTimer render_timer;

  qb_coro_sync(print_timing_info, qbVoid(&timing_info));

  gui::qbWindow test_window;
  gui::qb_window_create(&test_window, { 1200.0f - 512.0f, 800 - 512.0f, -0.5f }, { 512.0f, 512.0f }, true);
  gui::qb_window_create(&test_window, { 1000.0f - 512.0f, 800 - 512.0f, 0.9f }, { 512.0f, 512.0f }, true);

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

    accumulator += frame_time;
    
    input::handle_input([](SDL_Event*) {
      shutdown();
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


    {
      Matrices mat = {};
      mat.mvp = glm::mat4(1.0f);
      glm::mat4 view = glm::mat4(1.0f);
      // note that we're translating the scene in the reverse direction of where we want to move
      view = glm::translate(view, glm::vec3(-1.0f, 0.0f, -5.0f));

      glm::mat4 model = glm::mat4(1.0f);
      model = glm::rotate(model, (float)frame / 75.0f, glm::vec3(1.0f, 0.0f, 0.0f));

      mat.mvp = view * model;// glm::rotate(mat.mvp, (float)frame / 50.0f, glm::vec3(0.0f, 0.0f, 1.0f));
      qb_gpubuffer_update(model_ubo[0], 0, sizeof(Matrices), &mat);
    }

    {
      Matrices mat = {};
      mat.mvp = glm::mat4(1.0f);
      glm::mat4 view = glm::mat4(1.0f);
      // note that we're translating the scene in the reverse direction of where we want to move
      view = glm::translate(view, glm::vec3(1.0f, 0.0f, -5.0f));

      glm::mat4 model = glm::mat4(1.0f);
      model = glm::rotate(model, (float)frame / 75.0f, glm::vec3(1.0f, 0.0f, 0.0f));

      mat.mvp = view * model;// glm::rotate(mat.mvp, (float)frame / 50.0f, glm::vec3(0.0f, 0.0f, 1.0f));
      qb_gpubuffer_update(model_ubo[1], 0, sizeof(Matrices), &mat);
    }

    qb_timer_start(render_timer);

    qbRenderEvent e;
    e.frame = frame;
    e.alpha = accumulator / dt;
    //qb_render(&e);
    qb_renderpipeline_run(render_pipeline, e);
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
  }
}
