/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "forward_renderer.h"

#include <cubez/render.h>
#include <cubez/mesh.h>
#include <map>
#include <iostream>
#include <array>
#include <assert.h>
#include <cglm/struct.h>

const size_t kMaxDirectionalLights = 4;
const size_t kMaxPointLights = 32;
const size_t kMaxSpotLights = 8;

struct LightPoint {
  // Interleave the brightness and radius to optimize std140 wasted bytes.
  vec3s rgb;
  float brightness;
  vec3s pos;
  float radius;
};

typedef struct LightDirectional {
  vec3s rgb;
  float brightness;
  vec3s dir;
  float _dir_pad;
};

typedef struct LightSpot {
  vec3s rgb;
  float brightness;
  vec3s pos;
  float range;
  vec3s dir;
  float angle_deg;
};

struct SdfVArgs {
  vec4s unused;
};

struct SdfFArgs {
  alignas(16) mat4s vp_inv;
  alignas(16) vec3s eye;
};

typedef struct qbForwardRenderer_ {
  qbRenderer_ renderer;

  qbRenderPass scene_3d_pass;
  qbRenderPass scene_2d_pass;
  qbRenderPass gui_pass;

  qbFrameBuffer frame_buffer;

  qbGeometryDescriptor supported_geometry = nullptr;  

  qbSystem render_system;

  // The first binding of user textures.
  size_t texture_start_binding;
  size_t texture_units_count;

  // The first binding of user uniforms.
  size_t uniform_start_binding;
  size_t uniform_count;

  uint32_t camera_uniform;
  std::string camera_uniform_name;
  qbGpuBuffer camera_ubo;

  // Created per rendergroup.
  uint32_t model_uniform;
  std::string model_uniform_name;

  // Created per rendergroup.
  uint32_t material_uniform;
  std::string material_uniform_name;

  uint32_t light_uniform;
  std::string light_uniform_name;
  qbGpuBuffer light_ubo;

  std::array<struct LightDirectional, kMaxDirectionalLights> directional_lights;
  std::array<bool, kMaxDirectionalLights> enabled_directional_lights;

  std::array<struct LightPoint, kMaxPointLights> point_lights;
  std::array<bool, kMaxPointLights> enabled_point_lights;

  std::array<struct LightSpot, kMaxSpotLights> spot_lights;
  std::array<bool, kMaxSpotLights> enabled_spot_lights;

  size_t albedo_map_binding;
  size_t normal_map_binding;
  size_t metallic_map_binding;
  size_t roughness_map_binding;
  size_t ao_map_binding;
  size_t emission_map_binding;

  qbImage empty_albedo_map;
  qbImage empty_normal_map;
  qbImage empty_metallic_map;
  qbImage empty_roughness_map;
  qbImage empty_ao_map;
  qbImage empty_emission_map;

  qbSurface blur_surface;
  qbGpuBuffer blur_ubo;

  qbSurface merge_surface;
  qbGpuBuffer merge_ubo;

  // https://0xef.wordpress.com/2013/01/19/ray-marching-signed-distance-fields/
  // https://shaderbits.com/blog/creating-volumetric-ray-marcher  
  // https://www.reddit.com/r/opengl/comments/2ui4i0/depth_test_using_multiple_depth_buffers_at_once/
  // http://jamie-wong.com/2016/07/15/ray-marching-signed-distance-functions/
  // http://iquilezles.org/www/articles/distfunctions/distfunctions.htm
  // https://casual-effects.com/research/McGuire2019ProcGen/McGuire2019ProcGen.pdf
  // https://www.shadertoy.com/view/lt3XDM
  qbSurface sdf_surface;
  qbGpuBuffer sdf_vubo;
  qbGpuBuffer sdf_fubo;
  qbImage sdf_layers;

  // Voxel
  // https://0fps.net/category/programming/voxels/
  // https://forum.openframeworks.cc/t/gpu-marching-cubes-example/9129
  // https://github.com/chriskiefer/MarchingCubesGPU/blob/master/src/marchingCubesGPU.cpp
  // http://www.icare3d.org/codes-and-projects/codes/opengl_geometry_shader_marching_cubes.html
  // http://users.belgacom.net/gc610902/
  // https://github.com/dominikwodniok/dualmc
} qbForwardRenderer_, *qbForwardRenderer;

struct qbBlurArgs {
  int32_t horizontal;
};

struct qbMergeArgs {
  int32_t bloom;
  float exposure;
};

struct CameraUniform {
  alignas(16) mat4s vp;
};

struct ModelUniform {
  alignas(16) mat4s m;
  alignas(16) mat4s rot;
};

struct MaterialUniform {
  vec3s albedo;
  float metallic;
  vec3s emission;
  float roughness;
};

struct LightUniform {
  LightDirectional directionals[kMaxDirectionalLights];
  LightPoint points[kMaxPointLights];
  LightSpot spots[kMaxSpotLights];

  vec3s view_pos;
  float _view_pos_pad;
};

void rendergroup_oncreate(struct qbRenderer_* self, qbRenderGroup group) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  
  qbGpuBuffer model;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(ModelUniform);
    attr.name = r->model_uniform_name.data();
    qb_gpubuffer_create(&model, &attr);
  }
  
  qbGpuBuffer material;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.size = sizeof(ModelUniform);
    attr.name = r->material_uniform_name.data();
    qb_gpubuffer_create(&material, &attr);
  }

  uint32_t bindings[] = { ((qbForwardRenderer)self)->model_uniform,
                          ((qbForwardRenderer)self)->material_uniform };
  qbGpuBuffer uniforms[] = { model, material };
  qb_rendergroup_attachuniforms(group, sizeof(uniforms) / sizeof(qbGpuBuffer),
                                bindings, uniforms);
}

void rendergroup_ondestroy(struct qbRenderer_* self, qbRenderGroup group) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  qb_rendergroup_removeuniform_bybinding(group, r->model_uniform);
}

void model_add(qbRenderer self, qbRenderGroup model) {
  qb_renderpass_append(((qbForwardRenderer)self)->scene_3d_pass, model);
}

void model_remove(qbRenderer self, qbRenderGroup model) {
  qb_renderpass_remove(((qbForwardRenderer)self)->scene_3d_pass, model);
}

void update_camera_ubo(qbForwardRenderer r, const struct qbCamera_* camera) {
  CameraUniform m;
  m.vp = glms_mat4_mul(camera->projection_mat, camera->view_mat);
  qb_gpubuffer_update(r->camera_ubo, 0, sizeof(CameraUniform), &m);
}

void update_light_ubo(qbForwardRenderer r, const struct qbCamera_* camera) {
  LightUniform l = {};
  for (size_t i = 0; i < r->directional_lights.size(); ++i) {
    if (r->enabled_directional_lights[i]) {
      l.directionals[i] = r->directional_lights[i];
    }
  }

  for (size_t i = 0; i < r->point_lights.size(); ++i) {
    if (r->enabled_point_lights[i]) {
      l.points[i] = r->point_lights[i];
    }
  }

  for (size_t i = 0; i < r->spot_lights.size(); ++i) {
    if (r->enabled_spot_lights[i]) {
      l.spots[i] = r->spot_lights[i];
    }
  }
  l.view_pos = camera->origin;

  qb_gpubuffer_update(r->light_ubo, 0, sizeof(LightUniform), &l);
}

void update_sdf_ubo(qbForwardRenderer r, const struct qbCamera_* camera) {
  SdfVArgs v_args = {};
  SdfFArgs f_args = {};

  mat4s project_view = glms_mat4_mul(camera->projection_mat, camera->view_mat);
  f_args.vp_inv = glms_mat4_inv(project_view);
  f_args.eye = camera->origin;

  qb_gpubuffer_update(r->sdf_vubo, 0, sizeof(v_args), &v_args);
  qb_gpubuffer_update(r->sdf_fubo, 0, sizeof(f_args), &f_args);
}

void render(struct qbRenderer_* self, const struct qbCamera_* camera, qbRenderEvent event) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  qbFrameBuffer camera_fbo = qb_camera_fbo(camera);

  update_camera_ubo(r, camera);
  update_light_ubo(r, camera);
  update_sdf_ubo(r, camera);
  {
    qbClearValue_ clear = {};
    clear.attachments = qbFrameBufferAttachment(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT);
    clear.color = vec4s{ 0.0f, 0.0f, 0.0f, 1.0f };
    clear.depth = 1.0f;
    qb_framebuffer_clear(camera_fbo, &clear);
  }
  qb_renderpass_draw(r->scene_3d_pass, camera_fbo);

  if (1)
  {
    {
      qbBlurArgs args;
      args.horizontal = 1;
      qb_gpubuffer_update(r->blur_ubo, 0, sizeof(qbBlurArgs), &args);

      qbImage read[] = { qb_framebuffer_target(camera_fbo, 1) };
      qbFrameBuffer write = qb_surface_target(r->blur_surface, 0);
      qb_surface_draw(r->blur_surface, read, write);
    }

    for (int i = 0; i < 10; ++i) {
      qbBlurArgs args;
      args.horizontal = i % 2;
      qb_gpubuffer_update(r->blur_ubo, 0, sizeof(qbBlurArgs), &args);

      qbImage read[] = { qb_framebuffer_target(qb_surface_target(r->blur_surface, i % 2), 0) };
      qbFrameBuffer write = qb_surface_target(r->blur_surface, (i + 1) % 2);
      qb_surface_draw(r->blur_surface, read, write);
    }
  }
  if (1)
  {
    qbFrameBuffer blur_frame = qb_surface_target(r->blur_surface, 0);
    qbImage read[] = {
      qb_framebuffer_target(blur_frame, 0),
      qb_framebuffer_target(camera_fbo, 0)
    };
    qbFrameBuffer write = qb_surface_target(r->merge_surface, 0);

    qbMergeArgs args;
    args.bloom = true;
    args.exposure = 1.0f;
    qb_gpubuffer_update(r->merge_ubo, 0, sizeof(qbMergeArgs), &args);
    
    qb_surface_draw(r->merge_surface, read, write);
  }

  qbFrameBuffer final = qb_surface_target(r->merge_surface, 0);

  qb_surface_draw(r->sdf_surface, &r->sdf_layers, final);
  qb_renderpass_draw(r->gui_pass, final);

  qb_renderpipeline_present(self->render_pipeline, final, event);
}

void render_callback(qbFrame* f) {
  qbRenderEvent event = (qbRenderEvent)f->event;
  render(event->renderer, event->camera, event);
}

qbMeshBuffer meshbuffer_create(qbRenderer self, qbMesh mesh) {
  qbGpuBuffer verts;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = mesh->vertices;
    attr.elem_size = sizeof(float);
    attr.size = mesh->vertex_count * sizeof(vec3s);
    qb_gpubuffer_create(&verts, &attr);
  }

  qbGpuBuffer normals;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = mesh->normals;
    attr.elem_size = sizeof(float);
    attr.size = mesh->normal_count * sizeof(vec3s);
    qb_gpubuffer_create(&normals, &attr);
  }

  qbGpuBuffer uvs;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = mesh->uvs;
    attr.elem_size = sizeof(float);
    attr.size = mesh->uv_count * sizeof(vec2s);
    qb_gpubuffer_create(&uvs, &attr);
  }

  qbGpuBuffer indices;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = mesh->indices;
    attr.elem_size = sizeof(uint32_t);
    attr.size = mesh->index_count * sizeof(uint32_t);
    qb_gpubuffer_create(&indices, &attr);
  }

  qbMeshBufferAttr_ attr = {};
  attr.descriptor = *((qbForwardRenderer)self)->supported_geometry;

  qbMeshBuffer ret;
  qb_meshbuffer_create(&ret, &attr);

  qbGpuBuffer buffers[] = { verts, normals, uvs };
  qb_meshbuffer_attachvertices(ret, buffers);
  qb_meshbuffer_attachindices(ret, indices);
  return ret;
}

void meshbuffer_attach_textures(qbRenderer self, qbMeshBuffer buffer,
                                size_t count,
                                uint32_t texture_units[],
                                qbImage textures[]) {
  uint32_t tex_units[16] = {};

  qbForwardRenderer r = (qbForwardRenderer)self;
  assert(count <= r->texture_units_count && count < 16 && "Unsupported amount of textures");

  for (size_t i = 0; i < count; ++i) {
    uint32_t texture_unit = texture_units[i];
    assert(texture_unit < r->texture_units_count && "Texture unit out of bounds");
    tex_units[i] = texture_unit + r->texture_start_binding;
  }

  qb_meshbuffer_attachimages(buffer, count, tex_units, textures);
}

void meshbuffer_attach_uniforms(qbRenderer self, qbMeshBuffer buffer,
                                size_t count,
                                uint32_t uniform_bindings[],
                                qbGpuBuffer uniforms[]) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  std::vector<uint32_t> bindings;
  bindings.resize(count);

  for (size_t i = 0; i < count; ++i) {
    bindings[i] = uniform_bindings[i] + r->uniform_start_binding;
  }
  qb_meshbuffer_attachuniforms(buffer, count, bindings.data(), uniforms);
}

void rendergroup_attach_material(struct qbRenderer_* self, qbRenderGroup group,
                                 struct qbMaterial_* material) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  
  uint32_t units[6] = {};
  qbImage images[6] = {};

  uint32_t count = 0;
  uint32_t material_tex_units[] = {
    r->albedo_map_binding,
    r->normal_map_binding,
    r->metallic_map_binding,
    r->roughness_map_binding,
    r->ao_map_binding,
    r->emission_map_binding,
  };

  qbImage material_images[] = {
    material->albedo_map ? material->albedo_map : r->empty_albedo_map,
    material->normal_map ? material->normal_map : r->empty_normal_map,
    material->metallic_map ? material->metallic_map : r->empty_metallic_map,
    material->roughness_map ? material->roughness_map : r->empty_roughness_map,
    material->ao_map ? material->ao_map : r->empty_ao_map,
    material->emission_map ? material->emission_map : r->empty_emission_map,
  };

  for (int i = 0; i < 6; ++i) {
    qbImage image = material_images[i];
    if (image) {
      units[count] = material_tex_units[i];
      images[count] = image;
      ++count;
    }
  }

  qb_rendergroup_attachimages(group, count, units, images);
}

void rendergroup_attach_textures(struct qbRenderer_* self, qbRenderGroup group,
                                 size_t count,
                                 uint32_t texture_units[],
                                 qbImage textures[]) {
  uint32_t tex_units[16] = {};

  qbForwardRenderer r = (qbForwardRenderer)self;
  assert(count <= r->texture_units_count && count < 16 && "Unsupported amount of textures");

  for (size_t i = 0; i < count; ++i) {
    uint32_t texture_unit = texture_units[i];
    assert(texture_unit < r->texture_units_count && "Texture unit out of bounds");
    tex_units[i] = texture_unit + r->texture_start_binding;
  }

  qb_rendergroup_attachimages(group, count, tex_units, textures);
}

void rendergroup_attach_uniforms(struct qbRenderer_* self, qbRenderGroup group,
                                 size_t count,
                                 uint32_t uniform_bindings[],
                                 qbGpuBuffer uniforms[]) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  std::vector<uint32_t> bindings;
  bindings.resize(count);

  for (size_t i = 0; i < count; ++i) {
    bindings[i] = uniform_bindings[i] + r->uniform_start_binding;
  }
  qb_rendergroup_attachuniforms(group, count, bindings.data(), uniforms);
}

void light_enable(struct qbRenderer_* self, qbId id, qbLightType light_type) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  switch (light_type) {
    case qbLightType::QB_LIGHT_TYPE_DIRECTIONAL:
      if (id < kMaxDirectionalLights) {
        r->enabled_directional_lights[id] = true;;
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
  qbForwardRenderer r = (qbForwardRenderer)self;
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
  qbForwardRenderer r = (qbForwardRenderer)self;
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
  qbForwardRenderer r = (qbForwardRenderer)self;
  if (id >= kMaxDirectionalLights) {
    return;
  }
  
  r->directional_lights[id] = LightDirectional{ rgb, brightness, dir };
}

void light_point(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s pos, float brightness, float range) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  if (id >= kMaxPointLights) {
    return;
  }

  r->point_lights[id] = LightPoint{ rgb, brightness, pos, range };
}

void light_spot(struct qbRenderer_* self, qbId id, vec3s rgb, vec3s pos, vec3s dir,
                float brightness, float range, float angle_deg) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  if (id >= kMaxSpotLights) {
    return;
  }

  r->spot_lights[id] = LightSpot{ rgb, brightness, pos, range, dir, angle_deg };
}

size_t light_max(struct qbRenderer_* self, qbLightType light_type) {
  qbForwardRenderer r = (qbForwardRenderer)self;
  
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


qbFrameBuffer camera_framebuffer_create(struct qbRenderer_* self, uint32_t width, uint32_t height) {
  qbFrameBufferAttr_ fbo_attr = {};
  fbo_attr.width = width;
  fbo_attr.height = height;

  qbFrameBufferAttachment attachments[] = {
    (qbFrameBufferAttachment)(QB_COLOR_ATTACHMENT | QB_DEPTH_ATTACHMENT),
    QB_COLOR_ATTACHMENT,
  };

  uint32_t bindings[] = { 0, 1 };

  fbo_attr.attachments = attachments;
  fbo_attr.color_binding = bindings;
  fbo_attr.attachments_count = sizeof(attachments) / sizeof(qbFrameBufferAttachment);

  qbFrameBuffer fbo;
  qb_framebuffer_create(&fbo, &fbo_attr);

  return fbo;
}

void init_supported_geometry(qbForwardRenderer forward_renderer) {
  // https://stackoverflow.com/questions/40450342/what-is-the-purpose-of-binding-from-vkvertexinputbindingdescription
  qbBufferBinding attribute_bindings = new qbBufferBinding_[3];
  {
    qbBufferBinding binding = attribute_bindings;
    binding->binding = 0;
    binding->stride = 3 * sizeof(float); // Vertex(x, y, z)
    binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
  }
  {
    qbBufferBinding binding = attribute_bindings + 1;
    binding->binding = 1;
    binding->stride = 3 * sizeof(float); // Normal(x, y, z)
    binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
  }
  {
    qbBufferBinding binding = attribute_bindings + 2;
    binding->binding = 2;
    binding->stride = 2 * sizeof(float); // Texture(u, v)
    binding->input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
  }
  /*{
    qbBufferBinding binding = attribute_bindings + 1;
    binding->binding = 3;
    binding->stride = 2 * sizeof(float);
    binding->input_rate = QB_VERTEX_INPUT_RATE_INSTANCE;
  }*/

  qbVertexAttribute attributes = new qbVertexAttribute_[4];
  {
    qbVertexAttribute_* attr = attributes;
    *attr = {};
    attr->binding = 0;
    attr->location = 0;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_VERTEX;
    attr->normalized = false;
    attr->offset = (void*)0;
  }
  {
    qbVertexAttribute_* attr = attributes + 1;
    *attr = {};
    attr->binding = 1;
    attr->location = 1;

    attr->count = 3;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_NORMAL;
    attr->normalized = false;
    attr->offset = (void*)(0 * sizeof(float));
  }
  {
    qbVertexAttribute_* attr = attributes + 2;
    *attr = {};
    attr->binding = 2;
    attr->location = 2;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_TEXTURE;
    attr->normalized = false;
    attr->offset = (void*)(0 * sizeof(float));
  }
  /*{
    qbVertexAttribute_* attr = attributes + 3;
    *attr = {};
    attr->binding = 3;
    attr->location = 3;

    attr->count = 2;
    attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
    attr->name = QB_VERTEX_ATTRIB_NAME_PARAM;
    attr->normalized = false;
    attr->offset = (void*)0;
  }*/

  forward_renderer->supported_geometry = new qbGeometryDescriptor_;
  qbGeometryDescriptor supported_geometry = forward_renderer->supported_geometry;
  supported_geometry->attributes = attributes;
  supported_geometry->attributes_count = 3;
  supported_geometry->bindings = attribute_bindings;
  supported_geometry->bindings_count = 3;
}

qbSurface create_blur_surface(qbForwardRenderer r, uint32_t width, uint32_t height) {
  qbSurfaceAttr_ attr;
  attr.width = width;
  attr.height = height;
  attr.vs = "resources/blur.vs";
  attr.fs = "resources/blur.fs";

  qbShaderResourceInfo_ resources[2] = {};
  {
    qbShaderResourceInfo info = resources;
    info->binding = 0;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_blur_args";
  }
  {
    qbShaderResourceInfo info = resources + 1;
    info->binding = 1;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_blur_sampler";
  }
  attr.resources = resources;
  attr.resources_count = sizeof(resources) / sizeof(resources[0]);

  qbImageSampler sampler;
  {
    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;
    attr.s_wrap = qbImageWrapType::QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
    attr.t_wrap = qbImageWrapType::QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE;
    qb_imagesampler_create(&sampler, &attr);
  }
  uint32_t sampler_bindings[] = { 1 };
  qbImageSampler samplers[] = { sampler };
  attr.samplers = samplers;
  attr.sampler_bindings = sampler_bindings;
  attr.samplers_count = 1;

  qbGpuBuffer ubo;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(qbBlurArgs);

    qb_gpubuffer_create(&r->blur_ubo, &attr);
  }
  uint32_t ubo_bindings[] = { 0 };
  qbGpuBuffer uniforms[] = { r->blur_ubo };
  attr.uniforms = uniforms;
  attr.uniform_bindings = ubo_bindings;
  attr.uniforms_count = 1;

  attr.pixel_format = QB_PIXEL_FORMAT_RGBA8;
  attr.target_count = 2;

  qbSurface ret;
  qb_surface_create(&ret, &attr);
  return ret;
}

float cube_sdf(vec3s p, vec3s c, float size) {
  // If d.x < 0, then -1 < p.x < 1, and same logic applies to p.y, p.z
  // So if all components of d are negative, then p is inside the unit cube
  vec3s d = glms_vec3_sub(glms_vec3_abs(glms_vec3_sub(p, c)), vec3s{ size * 0.5f, size * 0.5f, size * 0.5f });

  // Assuming p is inside the cube, how far is it from the surface?
  // Result will be negative or zero.
  float insideDistance = glm_min(glm_max(d.x, glm_max(d.y, d.z)), 0.0);

  // Assuming p is outside the cube, how far is it from the surface?
  // Result will be positive or zero.
  d.x = glm_max(d.x, 0.0f);
  d.y = glm_max(d.y, 0.0f);
  d.z = glm_max(d.z, 0.0f);
  float outsideDistance = glms_vec3_norm(d);

  return insideDistance + outsideDistance;
}

float sphere_sdf(vec3s p, vec3s c, float radius) {
  return glms_vec3_norm(glms_vec3_sub(p, c)) - radius;
}

qbSurface create_sdf_surface(qbForwardRenderer r, uint32_t width, uint32_t height) {
  qbSurfaceAttr_ attr = {};
  attr.width = width;
  attr.height = height;
  attr.vs = "resources/sdf.vs";
  attr.fs = "resources/sdf.fs";

  qbShaderResourceInfo_ resources[3] = {};
  {
    qbShaderResourceInfo info = resources;
    info->binding = 0;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info->stages = QB_SHADER_STAGE_VERTEX;
    info->name = "qb_sdf_vargs";
  }
  {
    qbShaderResourceInfo info = resources + 1;
    info->binding = 1;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_sdf_fargs";
  }
  {
    qbShaderResourceInfo info = resources + 2;
    info->binding = 2;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_layer_sampler";
  }
  attr.resources = resources;
  attr.resources_count = sizeof(resources) / sizeof(resources[0]);

  qbImageSampler samplers[1];
  {
    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_3D;
    attr.min_filter = QB_FILTER_TYPE_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;
    qb_imagesampler_create(samplers, &attr);
  }
  uint32_t sampler_bindings[] = { 2 };
  attr.samplers = samplers;
  attr.sampler_bindings = sampler_bindings;
  attr.samplers_count = 1;

  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(SdfVArgs);

    qb_gpubuffer_create(&r->sdf_vubo, &attr);
  }
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(SdfFArgs);

    qb_gpubuffer_create(&r->sdf_fubo, &attr);
  }
  uint32_t ubo_bindings[] = { 0, 1 };
  qbGpuBuffer uniforms[] = { r->sdf_vubo, r->sdf_fubo };
  attr.uniforms = uniforms;
  attr.uniform_bindings = ubo_bindings;
  attr.uniforms_count = 2;

  {
    typedef struct {
      int r : 8;
      int g : 8;
      int b : 8;
      int a : 8;
    } rgba_t;

    int size = 512;
    float size_f = (float)size;
    float radius = size_f / 2.5f;
    int w, h, d;
    w = h = d = size;
    vec3s c = { size / 2.0f, size / 2.0f, size / 2.0f };
    rgba_t* terrain_data = new rgba_t[w * h * d];
    srand(0);
    for (int k = 0; k < d; ++k) {
      for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
          size_t idx = i + j * w + k * w * h;
          vec3s p = glms_vec3_sub({ (float)i, (float)j, (float)k }, c);
          
          if (sphere_sdf({ (float)i, (float)j, (float)k }, c, radius) < 0.0f) {
            terrain_data[idx] = { rand() % 255, 0x00, 0x00, 0xFF };  
          } else {
            terrain_data[idx] = { 0x00, 0x00, 0x00, 0x00 };
          }


          /*vec3s p = glms_vec3_sub({ (float)i, (float)j, (float)k }, c);
          float r = glms_vec3_norm(p);

          terrain_data[idx] = 
            glm_max(
              sphere_sdf({ (float)i, (float)j, (float)k }, c, radius),// + (float)(rand() % 100) / 100.0f,
              //r - size_f / 2.5f + (float)(rand() % 100) / 10.0f,
              -cube_sdf(p, vec3s{ 64, 0, 0 }, size_f / 2.5f)
              ) / size_f;*/
        }
      }
    }

    qbPixelMap terrain = qb_pixelmap_create(size, size, size, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, terrain_data);
    
    qbImageAttr_ image_attr = {};
    image_attr.type = qbImageType::QB_IMAGE_TYPE_3D;
    image_attr.generate_mipmaps = false;
    qb_image_create(&r->sdf_layers, &image_attr, terrain);
    //qb_image_load(&r->sdf_layers, &image_attr, "resources/soccer_ball.bmp");

    delete[] terrain_data;
    qb_pixelmap_destroy(&terrain);
  }

  qbSurface ret;
  qb_surface_create(&ret, &attr);
  return ret;
}

qbSurface create_merge_surface(qbForwardRenderer r, uint32_t width, uint32_t height) {
  qbSurfaceAttr_ attr;
  attr.width = width;
  attr.height = height;
  attr.vs = "resources/merge.vs";
  attr.fs = "resources/merge.fs";

  qbShaderResourceInfo_ resources[3] = {};
  {
    qbShaderResourceInfo info = resources;
    info->binding = 0;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_merge_args";
  }
  {
    qbShaderResourceInfo info = resources + 1;
    info->binding = 1;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_merge_sampler";
  }
  {
    qbShaderResourceInfo info = resources + 2;
    info->binding = 2;
    info->resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info->stages = QB_SHADER_STAGE_FRAGMENT;
    info->name = "qb_scene_sampler";
  }
  attr.resources = resources;
  attr.resources_count = sizeof(resources) / sizeof(resources[0]);

  qbImageSampler samplers[2];
  {
    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;
    qb_imagesampler_create(samplers, &attr);
  }
  {
    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;
    qb_imagesampler_create(samplers + 1, &attr);
  }
  uint32_t sampler_bindings[] = { 1, 2 };
  attr.samplers = samplers;
  attr.sampler_bindings = sampler_bindings;
  attr.samplers_count = 2;

  qbGpuBuffer ubo;
  {
    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(qbMergeArgs);

    qb_gpubuffer_create(&r->merge_ubo, &attr);
  }
  uint32_t ubo_bindings[] = { 0 };
  qbGpuBuffer uniforms[] = { r->merge_ubo };
  attr.uniforms = uniforms;
  attr.uniform_bindings = ubo_bindings;
  attr.uniforms_count = 1;

  attr.pixel_format = QB_PIXEL_FORMAT_RGBA8;
  attr.target_count = 1;

  qbSurface ret;
  qb_surface_create(&ret, &attr);
  return ret;
}

struct qbRenderer_* qb_forwardrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args) {
  qbForwardRenderer ret = new qbForwardRenderer_;

  ret->renderer.width = width;
  ret->renderer.height = height;
  ret->renderer.rendergroup_oncreate = rendergroup_oncreate;
  ret->renderer.rendergroup_ondestroy = rendergroup_ondestroy;
  ret->renderer.rendergroup_add = model_add;
  ret->renderer.rendergroup_remove = model_remove;
  ret->renderer.rendergroup_attach_textures = rendergroup_attach_textures;
  ret->renderer.rendergroup_attach_uniforms = rendergroup_attach_uniforms;
  ret->renderer.meshbuffer_create = meshbuffer_create;
  ret->renderer.meshbuffer_attach_textures = meshbuffer_attach_textures;
  ret->renderer.rendergroup_attach_material = rendergroup_attach_material;
  ret->gui_pass = args->opt_gui_renderpass;
  
  ret->renderer.light_enable = light_enable;
  ret->renderer.light_disable = light_disable;
  ret->renderer.light_isenabled = light_isenabled;
  ret->renderer.light_directional = light_directional;
  ret->renderer.light_point = light_point;
  ret->renderer.light_spot = light_spot;
  ret->renderer.light_max = light_max;
  ret->renderer.camera_framebuffer_create = camera_framebuffer_create;

  ret->renderer.render = render;
  ret->directional_lights = {};
  ret->enabled_directional_lights = {};
  ret->point_lights = {};
  ret->enabled_point_lights = {};
  ret->spot_lights = {};
  ret->enabled_spot_lights = {};
  
  {
    // If there is a sampler defined, there needs to be a corresponding image,
    // otherwise there is undetermined behavior. In the case that there isn't a
    // image given, an empty image is placed in its stead.
    char pixel_0[4] = {};
    qbPixelMap map_0 = qb_pixelmap_create(1, 1, 0, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, pixel_0);

    char pixel_1[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    qbPixelMap map_1 = qb_pixelmap_create(1, 1, 0, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, pixel_1);

    char pixel_n[4] = { 0x80, 0x80, 0xFF, 0xFF };
    qbPixelMap map_n = qb_pixelmap_create(1, 1, 0, qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, pixel_n);

    qbImageAttr_ attr = {};
    attr.type = qbImageType::QB_IMAGE_TYPE_2D;
    
    qb_image_create(&ret->empty_albedo_map, &attr, map_1);
    qb_image_create(&ret->empty_normal_map, &attr, map_n);
    qb_image_create(&ret->empty_metallic_map, &attr, map_0);
    qb_image_create(&ret->empty_roughness_map, &attr, map_0);
    qb_image_create(&ret->empty_ao_map, &attr, map_0);
    qb_image_create(&ret->empty_emission_map, &attr, map_0);
  }
  {
    qbRenderPipelineAttr_ attr = {};
    attr.viewport = { 0.0f, 0.0f, (float)width, (float)height };
    attr.viewport_scale = 1.0f;
    attr.name = "qbForwardRenderer";

    qb_renderpipeline_create(&ret->renderer.render_pipeline, &attr);
  }

  init_supported_geometry(ret);

  qbShaderModule shader_module;
  
  std::vector<qbShaderResourceInfo_> resources;
  std::vector<uint32_t> uniform_bindings;
  std::vector<std::pair<qbShaderResourceInfo_, qbGpuBuffer>> resource_uniforms;
  std::vector<uint32_t> sampler_bindings;
  std::vector<std::pair<qbShaderResourceInfo_, qbImageSampler>> resource_samplers;

  size_t native_uniform_count;
  size_t native_sampler_count;
  
  size_t uniform_start_binding;
  size_t user_uniform_count = args->uniform_count;

  size_t sampler_start_binding;
  size_t user_sampler_count = args->image_sampler_count;

  ret->camera_uniform_name = "qb_camera";
  ret->model_uniform_name = "qb_model";
  ret->material_uniform_name = "qb_material";
  ret->light_uniform_name = "qb_lights";

  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_VERTEX;
    info.name = ret->camera_uniform_name.data();

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(CameraUniform);

    qb_gpubuffer_create(&ret->camera_ubo, &attr);
    resource_uniforms.push_back({ info, ret->camera_ubo });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_VERTEX;
    info.name = ret->model_uniform_name.data();

    resource_uniforms.push_back({ info, nullptr });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = ret->material_uniform_name.data();

    resource_uniforms.push_back({ info, nullptr });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = ret->light_uniform_name.data();


    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(LightUniform);

    qb_gpubuffer_create(&ret->light_ubo, &attr);
    resource_uniforms.push_back({ info, ret->light_ubo });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = "qb_texture_unit_1";

    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;

    qbImageSampler sampler;
    qb_imagesampler_create(&sampler, &attr);

    //resource_samplers.push_back({ info, sampler });
  }
  {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = "qb_texture_unit_2";

    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;

    qbImageSampler sampler;
    qb_imagesampler_create(&sampler, &attr);

    //resource_samplers.push_back({ info, sampler });
  }

  std::vector<std::string> material_sampler_names = {
    "qb_material_albedo_map",
    "qb_material_normal_map",
    "qb_material_metallic_map",
    "qb_material_roughness_map",
    "qb_material_ao_map",
    "qb_material_emission_map",
  };
  for (auto& sampler_name : material_sampler_names) {
    qbShaderResourceInfo_ info = {};
    info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
    info.stages = QB_SHADER_STAGE_FRAGMENT;
    info.name = sampler_name.data();

    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;

    qbImageSampler sampler;
    qb_imagesampler_create(&sampler, &attr);

    resource_samplers.push_back({ info, sampler });
  }

  native_uniform_count = resource_uniforms.size();
  native_sampler_count = resource_samplers.size();
  {
    std::vector<std::pair<qbShaderResourceInfo_, qbGpuBuffer>> user_uniforms;
    user_uniforms.resize(args->shader_resource_count);
    for (size_t i = 0; i < args->shader_resource_count; ++i) {
      qbShaderResourceInfo info = args->shader_resources + i;
      uint32_t binding = info->binding;
      user_uniforms[binding].first = *info;
    }
    for (size_t i = 0; i < args->uniform_count; ++i) {
      qbGpuBuffer uniform = args->uniforms[i];
      uint32_t binding = args->uniform_bindings[i];
      user_uniforms[binding].second = uniform;
    }
    for (auto&& e : user_uniforms) {
      resource_uniforms.push_back(e);
    }

    std::vector<std::pair<qbShaderResourceInfo_, qbImageSampler>> user_samplers;
    user_samplers.resize(args->image_sampler_count);
    for (size_t i = 0; i < args->image_sampler_count; ++i) {
      qbImageSampler sampler = args->image_samplers[i];

      qbShaderResourceInfo_ info;
      info.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      info.stages = QB_SHADER_STAGE_FRAGMENT;
      info.name = qb_imagesampler_name(sampler);

      user_samplers.push_back({ info, sampler });
    }
    for (auto&& e : user_samplers) {
      resource_samplers.push_back(e);
    }
  }

  uniform_bindings.reserve(resource_uniforms.size());
  sampler_bindings.reserve(resource_samplers.size());
  std::vector<qbGpuBuffer> uniforms;
  std::vector<qbImageSampler> samplers;
  {
    uint32_t binding = 0;

    size_t uniform_index = 0;
    for (auto& e : resource_uniforms) {
      e.first.binding = binding++;
      resources.push_back(e.first);

      if (e.second) {
        uniforms.push_back(e.second);
        uniform_bindings.push_back(e.first.binding);
      }

      if (std::string(e.first.name) == ret->camera_uniform_name) {
        ret->camera_uniform = e.first.binding;
      } else if (std::string(e.first.name) == ret->model_uniform_name) {
        ret->model_uniform = e.first.binding;
      } else if (std::string(e.first.name) == ret->material_uniform_name) {
        ret->material_uniform = e.first.binding;
      } else if (std::string(e.first.name) == ret->light_uniform_name) {
        ret->light_uniform = e.first.binding;
      }

      if (uniform_index == native_uniform_count) {
        uniform_start_binding = binding;
      }
      ++uniform_index;
    }

    size_t sampler_index = 0;
    for (auto& e : resource_samplers) {
      e.first.binding = binding;
      resources.push_back(e.first);
      samplers.push_back(e.second);
      sampler_bindings.push_back(binding++);

      if (std::string(e.first.name) == "qb_material_albedo_map") {
        ret->albedo_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_normal_map") {
        ret->normal_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_metallic_map") {
        ret->metallic_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_roughness_map") {
        ret->roughness_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_ao_map") {
        ret->ao_map_binding = e.first.binding;
      } else if (std::string(e.first.name) == "qb_material_emission_map") {
        ret->emission_map_binding = e.first.binding;
      }

      if (sampler_index == native_sampler_count) {
        sampler_start_binding = binding;
      }
      ++sampler_index;
    }
  }


  {   
    qbShaderModuleAttr_ attr = {};
    attr.vs = "resources/pbr.vs";
    attr.fs = "resources/pbr.fs";
    attr.resources = resources.data();
    attr.resources_count = resources.size();

    qb_shadermodule_create(&shader_module, &attr);
    qb_shadermodule_attachuniforms(shader_module,
                                   uniform_bindings.size(),
                                   uniform_bindings.data(),
                                   uniforms.data());

    qb_shadermodule_attachsamplers(shader_module,
                                   sampler_bindings.size(),
                                   sampler_bindings.data(),
                                   samplers.data());
  }

  {
    qbClearValue_ clear = {};
    clear.attachments = (qbFrameBufferAttachment)(qbFrameBufferAttachment::QB_COLOR_ATTACHMENT |
                                                  qbFrameBufferAttachment::QB_DEPTH_ATTACHMENT);    
    clear.depth = 0.0;
    clear.color = { 1.0, 1.0, 0.0, 1.0 };

    qbRenderPassAttr_ attr = {};
    attr.viewport = { 0.0, 0.0, (float)width, (float)height };
    attr.supported_geometry = *ret->supported_geometry;
    attr.shader = shader_module;
    attr.viewport_scale = 1.0f;
    attr.clear = clear;

    qb_renderpass_create(&ret->scene_3d_pass, &attr);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, render_callback);
    qb_systemattr_addconst(attr, qb_renderable());
    qb_systemattr_addconst(attr, qb_material());
    qb_systemattr_addmutable(attr, qb_transform());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame* f) {
      qbRenderEvent event = (qbRenderEvent)f->event;
      qbForwardRenderer renderer = (qbForwardRenderer)event->renderer;

      qbRenderable* renderable;
      qbMaterial* material;
      qbTransform transform;
      qb_instance_getconst(insts[0], &renderable);
      qb_instance_getconst(insts[1], &material);
      qb_instance_getmutable(insts[2], &transform);

      qb_renderable_upload(*renderable, *material);
      qbRenderGroup group = qb_renderable_rendergroup(*renderable);
      qbGpuBuffer model_buffer = qb_rendergroup_finduniform_bybinding(group, renderer->model_uniform);
      qbGpuBuffer material_buffer = qb_rendergroup_finduniform_bybinding(group, renderer->material_uniform);

      ModelUniform model_uniform;
      mat4s orientation = glms_translate(transform->orientation, transform->pivot);
      mat4s pos = glms_translate(GLMS_MAT4_IDENTITY_INIT, transform->position);
      model_uniform.m = glms_mat4_mul(pos, orientation);
      model_uniform.rot = transform->orientation;
      qb_gpubuffer_update(model_buffer, 0, sizeof(ModelUniform), &model_uniform);

      MaterialUniform material_uniform;
      material_uniform.metallic = (*material)->metallic;
      material_uniform.roughness = (*material)->roughness;
      material_uniform.albedo = (*material)->albedo;
      material_uniform.emission = (*material)->emission;

      qb_gpubuffer_update(material_buffer, 0, sizeof(MaterialUniform), &material_uniform);
    });

    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_system_create(&ret->render_system, attr);
    qb_systemattr_destroy(&attr);
    
    qb_event_subscribe(qb_render_event(), ret->render_system);
  }

  ret->uniform_start_binding = uniform_start_binding;
  ret->uniform_count = user_uniform_count;
  ret->texture_start_binding = sampler_start_binding;
  ret->texture_units_count = user_sampler_count;
  ret->blur_surface = create_blur_surface(ret, width, height);
  ret->merge_surface = create_merge_surface(ret, width, height);
  ret->sdf_surface = create_sdf_surface(ret, width, height);
  ret->renderer.state = ret;
  return (qbRenderer)ret;
}

void qb_forwardrenderer_destroy(qbRenderer renderer) {
  qbForwardRenderer r = (qbForwardRenderer)renderer;
  qb_event_unsubscribe(qb_render_event(), r->render_system);
  qb_system_destroy(&r->render_system);
}

