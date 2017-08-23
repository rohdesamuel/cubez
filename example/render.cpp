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


// Collections
Objects::Table* objects;
qbCollection* objects_collection;

Renderables::Table* renderables;
qbCollection* renderables_collection;

Materials::Table* materials;
qbCollection* materials_collection;

Meshes::Table* meshes;
qbCollection* meshes_collection;

// Channels
qbChannel* insert_channel;
qbChannel* render_channel;

// Systems
qbSystem* insert_system;

// State
std::atomic_int next_id;

void initialize() {
  // Initialize collections.
  {
    std::cout << "Intializing render collections\n";
    objects_collection = Objects::Table::new_collection(kMainProgram, kCollection);
    objects = (Objects::Table*)objects_collection->collection;
  }
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

    insert_system = qb_alloc_system(kMainProgram, nullptr, kCollection);
    insert_system->transform = [](qbSystem*, qbFrame* f) {
      std::cout << "insert render info\n";
      qbMutation* mutation = &f->mutation;
      mutation->mutate_by = qbMutateBy::INSERT;
      Objects::Element* msg = (Objects::Element*)f->message.data;
      Objects::Element* el =
        (Objects::Element*)(mutation->element);
      *el = *msg;
    };
    qb_enable_system(insert_system, policy);
    qb_disable_system(insert_system);
  }

  // Initialize events.
  {
    std::cout << "Intializing render events\n";
    qbEventPolicy policy;
    policy.size = sizeof(Objects::Element);
    qb_create_event(kMainProgram, kInsert, policy);
    qb_subscribe_to(kMainProgram, kInsert, insert_system);
    insert_channel = qb_open_channel(kMainProgram, kInsert);
  }

  {
    qbEventPolicy policy;
    policy.size = sizeof(RenderEvent);
    qb_create_event(kMainProgram, kRender, policy);
    render_channel = qb_open_channel(kMainProgram, kRender);
  }

  std::cout << "Finished initializing render\n";
}

void render_event(struct qbSystem*, struct qbFrame*,
                  const struct qbCollections* sources,
                  const struct qbCollections*) {
  qbCollection* renderables = sources->collection[0];
  qbCollection* materials = sources->collection[1];
  qbCollection* transforms = sources->collection[2];
  qbCollection* meshes = sources->collection[3];

  uint64_t count = renderables->count(renderables);
  for (uint64_t i = 0; i < count; ++i) {
    Renderable* obj = (Renderable*)renderables->accessor(
        renderables, qbIndexedBy::OFFSET, &i);
    Material* material = (Material*)materials->accessor(
        materials, qbIndexedBy::HANDLE, &obj->material_id);
    Mesh* mesh = (Mesh*)meshes->accessor(
        meshes, qbIndexedBy::HANDLE, &obj->mesh_id);
    std::set<qbHandle>* transforms_v = &obj->transform_ids;

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBindVertexArray(mesh->vao);

    ShaderProgram shaders(material->shader_id);
    shaders.use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, material->texture_id);
    glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
    for (const qbHandle handle : *transforms_v) {
      physics::Object& transform = *(physics::Object*)transforms->accessor(
          transforms, qbIndexedBy::HANDLE, &handle);

      glm::mat4 mvp = glm::ortho(0.0f, 8.0f, 0.0f, 6.0f);
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

qbHandle insert(const Material&, const Mesh&, const std::set<qbHandle>&) {
  return 0;
}

qbId add_transform(qbId, qbHandle) {
  return 0;
}

qbId create(Object* render_info) {
  qbId new_id = next_id;
  ++next_id;

  qbMessage* msg = qb_alloc_message(insert_channel);
  Objects::Element el;
  el.indexed_by = qbIndexedBy::KEY;
  el.key = new_id;

  el.value = *render_info;
  *(Objects::Element*)msg->data = el;
  qb_send_message(msg);

  return new_id;
}

}  // namespace render
