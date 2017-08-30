#include "physics.h"
#include "render.h"
#include "shader.h"

#include "constants.h"

#include <atomic>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

namespace render {

// Event names
const char kInsert[] = "render_insert";
const char kTransformInsert[] = "render_transform_insert";


// Collections
Renderables::Table* renderables;
qbCollection* renderables_collection;

Materials::Table* materials;
qbCollection* materials_collection;

Meshes::Table* meshes;
qbCollection* meshes_collection;

// Channels
qbChannel* insert_channel;
qbChannel* insert_transform_channel;
qbChannel* render_channel;

// Systems
qbSystem* insert_system;
qbSystem* insert_transform_system;
qbSystem* render_system;

// State
std::atomic_int next_id;

struct InsertEvent {
  qbId material_id;
  qbId mesh_id;
  qbId renderable_id;

  Material material;
  Mesh mesh;
};

struct InsertTransformEvent {
  qbId renderable_id;
  qbId transform_id;
};

void render_event(struct qbSystem*, struct qbFrame*,
                  const struct qbCollections* sources,
                  const struct qbCollections*) {
  qbCollection* renderables = sources->collection[0];
  qbCollection* materials = sources->collection[1];
  qbCollection* meshes = sources->collection[2];
  qbCollection* transforms = sources->collection[3];

  uint64_t count = renderables->count(renderables);

  for (uint64_t i = 0; i < count; ++i) {
    Renderable* obj = (Renderable*)renderables->accessor(
        renderables, qbIndexedBy::OFFSET, &i);
    Material* material = (Material*)materials->accessor(
        materials, qbIndexedBy::KEY, &obj->material_id);
    Mesh* mesh = (Mesh*)meshes->accessor(
        meshes, qbIndexedBy::KEY, &obj->mesh_id);
    const std::set<qbId>& transforms_v = obj->transform_ids;

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBindVertexArray(mesh->vao);

    ShaderProgram shaders(material->shader_id);
    //shaders.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material->texture_id);
    glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
    glm::mat4 ortho = glm::ortho(0.0f, 8.0f, 0.0f, 6.0f);
    for (const qbId id : transforms_v) {
      physics::Object& transform = *(physics::Object*)transforms->accessor(
          transforms, qbIndexedBy::KEY, &id);

      glm::mat4 mvp = ortho;
      glm::vec3 p = transform.p;
      p = p + glm::vec3{1.0f, 1.0f, 0.0f};
      p.x *= 4.0f;
      p.y *= 3.0f;

      mvp = glm::translate(mvp, p);
      glUniformMatrix4fv(glGetUniformLocation(
            shaders.id(), "uMvp"), 1, GL_FALSE, (GLfloat*)&mvp);
      glDrawArrays(GL_TRIANGLES, 0, mesh->count);
    }
  }
}

void present(RenderEvent* event) {
  qbMessage* m = qb_alloc_message(render_channel);
  *(RenderEvent*)m->data = *event;
  
  qb_send_message(m);
  qb_flush_events(kMainProgram, kRender);
}

qbId create(const Material& material, const Mesh& mesh) {
  qbId new_id = next_id++;

  qbMessage* m = qb_alloc_message(insert_channel);

  InsertEvent event;
  event.material_id = new_id;
  event.mesh_id = new_id;
  event.renderable_id = new_id;

  event.material = material;
  event.mesh = mesh;
  *(InsertEvent*)m->data = event;

  qb_send_message(m);

  return new_id;
}

void insert_event(struct qbSystem*, struct qbFrame* frame,
                  const struct qbCollections*,
                  const struct qbCollections* sinks) {
  qbCollection* renderables = sinks->collection[0];
  qbCollection* materials = sinks->collection[1];
  qbCollection* meshes = sinks->collection[2];

  InsertEvent* event = (InsertEvent*)frame->message.data;
  frame->mutation.mutate_by = qbMutateBy::INSERT;

  {
    Materials::Element* e = (Materials::Element*)frame->mutation.element;
    e->key = event->material_id;
    e->value = std::move(event->material);
    e->indexed_by = qbIndexedBy::KEY;

    frame->mutation.mutate_by = qbMutateBy::INSERT;
    materials->mutate(materials, &frame->mutation);
  }

  {
    Meshes::Element* e = (Meshes::Element*)frame->mutation.element;
    e->key = event->mesh_id;
    e->value = std::move(event->mesh);
    e->indexed_by = qbIndexedBy::KEY;

    frame->mutation.mutate_by = qbMutateBy::INSERT;
    meshes->mutate(meshes, &frame->mutation);
  }

  {
    Renderable r;
    r.material_id = event->material_id;
    r.mesh_id = event->mesh_id;

    Renderables::Element* e = (Renderables::Element*)frame->mutation.element;
    e->key = event->renderable_id;
    e->value = std::move(r);
    e->indexed_by = qbIndexedBy::KEY;

    frame->mutation.mutate_by = qbMutateBy::INSERT;
    renderables->mutate(renderables, &frame->mutation);
  }
}

void insert_transform_event(struct qbSystem*, struct qbFrame* frame,
                  const struct qbCollections*,
                  const struct qbCollections* sinks) {
  qbCollection* renderables = sinks->collection[0];

  InsertTransformEvent* event = (InsertTransformEvent*)frame->message.data;
  frame->mutation.mutate_by = qbMutateBy::UPDATE;

  {
    Renderable r = *(Renderable*)renderables->accessor(renderables, qbIndexedBy::KEY, &event->renderable_id);
    r.transform_ids.insert(event->transform_id);

    Renderables::Element* e = (Renderables::Element*)frame->mutation.element;
    e->key = event->renderable_id;
    e->value = std::move(r);
    e->indexed_by = qbIndexedBy::KEY;

    frame->mutation.mutate_by = qbMutateBy::INSERT;
    renderables->mutate(renderables, &frame->mutation);
  }
}

void add_transform(qbId renderable_id, qbId transform_id) {
  qbMessage* m = qb_alloc_message(insert_transform_channel);

  InsertTransformEvent event;
  event.renderable_id = renderable_id;
  event.transform_id = transform_id;

  *(InsertTransformEvent*)m->data = event;

  qb_send_message(m);
}

void initialize() {
  // Initialize collections.
  next_id = 0;
  {
    std::cout << "Intializing renderables collection\n";
    renderables_collection = Renderables::Table::new_collection(kMainProgram, kRenderables);
    renderables = (Renderables::Table*)renderables_collection->collection;
  }
  {
    std::cout << "Intializing meshes collection\n";
    meshes_collection = Meshes::Table::new_collection(kMainProgram, kMeshes);
    meshes = (Meshes::Table*)meshes_collection->collection;
  }
  {
    std::cout << "Intializing materials collection\n";
    materials_collection = Materials::Table::new_collection(kMainProgram, kMaterials);
    materials = (Materials::Table*)materials_collection->collection;
  }

  // Initialize systems.
  {
    std::cout << "Intializing render systems\n";
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;

    insert_system = qb_alloc_system(kMainProgram, nullptr, kRenderables);
    qb_add_sink(insert_system, kMaterials);
    qb_add_sink(insert_system, kMeshes);

    insert_system->callback = insert_event;
    qb_enable_system(insert_system, policy);
  }

  {
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;

    insert_transform_system = qb_alloc_system(kMainProgram, nullptr, kRenderables);

    insert_transform_system->callback = insert_transform_event;
    qb_enable_system(insert_transform_system, policy);
  }

  {
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;

    render_system = qb_alloc_system(kMainProgram, kRenderables, nullptr);
    qb_add_source(render_system, kMaterials);
    qb_add_source(render_system, kMeshes);
    qb_add_source(render_system, physics::kCollection); // transforms

    render_system->callback = render_event;
    qb_enable_system(render_system, policy);
  }

  // Initialize events.
  {
    std::cout << "Intializing render events\n";
    qbEventPolicy policy;
    policy.size = sizeof(InsertEvent);
    qb_create_event(kMainProgram, kInsert, policy);
    qb_subscribe_to(kMainProgram, kInsert, insert_system);
    insert_channel = qb_open_channel(kMainProgram, kInsert);
  }

  {
    qbEventPolicy policy;
    policy.size = sizeof(InsertTransformEvent);
    qb_create_event(kMainProgram, kTransformInsert, policy);
    qb_subscribe_to(kMainProgram, kTransformInsert, insert_transform_system);
    insert_transform_channel = qb_open_channel(kMainProgram, kTransformInsert);
  }

  {
    qbEventPolicy policy;
    policy.size = sizeof(RenderEvent);
    qb_create_event(kMainProgram, kRender, policy);
    qb_subscribe_to(kMainProgram, kRender, render_system);
    render_channel = qb_open_channel(kMainProgram, kRender);
  }

  std::cout << "Finished initializing render\n";
}

}  // namespace render
