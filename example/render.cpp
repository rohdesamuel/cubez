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
qbComponent renderables;
qbComponent materials;
qbComponent meshes;

// Channels
qbEvent insert_event;
qbEvent insert_transform_event;
qbEvent render_event;

// Systems
qbSystem insert_system;
qbSystem insert_transform_system;
qbSystem render_system;

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

void render_event_handler(qbSystem, qbElement* elements,
                          qbCollectionInterface* collections) {
  qbCollectionInterface* transforms = &collections[0];

  Renderable* obj = (Renderable*)elements[0].value;
  Material* material = (Material*)elements[1].value;
  Mesh* mesh = (Mesh*)elements[2].value;
  const std::set<qbId>& transforms_v = obj->transform_ids;

  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBindVertexArray(mesh->vao);

  ShaderProgram shaders(material->shader_id);
  //shaders.use();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, material->texture_id);
  glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
  glm::mat4 ortho = glm::ortho(0.0f, 8.0f, 0.0f, 6.0f);
  for (qbId id : transforms_v) {
    physics::Transform& transform = *(physics::Transform*)transforms->by_key(
        transforms, &id);

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

void present(RenderEvent* event) {
  qb_event_send(render_event, event);
  qb_event_flush(render_event);
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

void insert_event(qbSystem, void* message, qbCollectionInterface* collections) {
  qbCollectionInterface* renderables = &collections[0];
  qbCollectionInterface* materials = &collections[1];
  qbCollectionInterface* meshes = &collections[2];

  InsertEvent* event = (InsertEvent*)message;
  {
    qbElement element;
    element.key = &event->material_id;
    element.value = &event->material;
    materials->insert(materials, &element);
  }

  {
    qbElement element;
    element.key = &event->mesh_id;
    element.value = &event->mesh;
    meshes->insert(meshes, &element);
  }

  {
    Renderable r;
    r.material_id = event->material_id;
    r.mesh_id = event->mesh_id;

    qbElement element;
    element.key = &event->renderable_id;
    element.value = &r;
    renderables->insert(renderables, &element);
  }
}

void insert_transform_event(qbSystem, void* message, qbCollectionInterface* collections) {
  qbCollectionInterface* renderables = &collections[0];

  InsertTransformEvent* event = (InsertTransformEvent*)message;

  Renderable r = *(Renderable*)renderables->by_key(renderables,
                                                   &event->renderable_id);
  r.transform_ids.insert(event->transform_id);

  qbElement element;
  element.key = &event->renderable_id;
  element.value = &r;

  renderables->insert(renderables, &element);
}

void add_transform(qbId renderable_id, qbId transform_id) {
  InsertTransformEvent event;
  event.renderable_id = renderable_id;
  event.transform_id = transform_id;

  qb_event_send(inter_event, &m);
}

void initialize() {
  // Initialize collections.
  next_id = 0;
  {
    std::cout << "Intializing renderables collection\n";
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setprogram(&attr, kMainProgram);
    qb_componentattr_setdatasize(&attr, sizeof(Renderable));

    qb_component_create(&renderables, "renderables", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    std::cout << "Intializing meshes collection\n";
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setprogram(&attr, kMainProgram);
    qb_componentattr_setdatasize(&attr, sizeof(Mesh));

    qb_component_create(&meshes, "meshes", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    std::cout << "Intializing materials collection\n";
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setprogram(&attr, kMainProgram);
    qb_componentattr_setdatasize(&attr, sizeof(Material));

    qb_component_create(&materials, "materials", attr);
    qb_componentattr_destroy(&attr);
  }

  // Initialize systems.
  {
    std::cout << "Intializing render systems\n";
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(&attr, kMainProgram);
    qb_systemattr_settrigger(&attr, qbTrigger::EVENT);
    qb_systemattr_setpriority(&attr, QB_MIN_PRIORITY);
    qb_systemattr_addsink(&attr, renderables);
    qb_systemattr_addsink(&attr, materials);
    qb_systemattr_addsink(&attr, meshes);
    qb_systemattr_setfunction(&attr, insert_event);

    qb_system_create(&insert_system, attr);
  }

  {
    qbSystemCreateInfo info;
    info.program = kMainProgram;

    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;
    info.policy = policy;

    qbCollection* sinks[] = { renderables };
    info.sources.collection = nullptr;
    info.sinks.collection = sinks;
    info.sinks.count = 1;
    
    info.transform = nullptr;
    info.callback = insert_transform_event;

    qb_alloc_system(&info, &insert_transform_system);
  }

  {
    qbSystemCreateInfo info;
    info.program = kMainProgram;

    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;
    info.policy = policy;

    qbCollection* sources[] = {renderables,
                               materials,
                               meshes,
                               physics::get_collection()};
    info.sources.collection = sources;
    info.sources.count = 4;
    info.sinks.collection = nullptr;
    info.transform = nullptr;
    info.callback = render_event;
    qb_alloc_system(&info, &render_system);
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
