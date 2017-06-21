#include "render.h"

#include "constants.h"

#include <atomic>
#include <SDL2/SDL_opengl.h>

namespace render {

// Event names
const char kInsert[] = "render_insert";


// Collections
Objects::Table* objects;
qbCollection* objects_collection;

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
    objects = new Objects::Table();
    objects_collection = qb_alloc_collection(kMainProgram, kCollection);
    objects_collection->collection = objects;

    objects_collection->accessor = Objects::Table::default_accessor;
    objects_collection->copy = Objects::Table::default_copy;
    objects_collection->mutate = Objects::Table::default_mutate;
    objects_collection->count = Objects::Table::default_count;

    objects_collection->keys.data = Objects::Table::default_keys;
    objects_collection->keys.size = sizeof(Objects::Key);
    objects_collection->keys.offset = 0;

    objects_collection->values.data = Objects::Table::default_values;
    objects_collection->values.size = sizeof(Objects::Value);
    objects_collection->values.offset = 0;
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

void present(RenderEvent* event) {
  qbMessage* m = qb_alloc_message(render_channel);
  *(RenderEvent*)m->data = *event;
  
  qb_send_message(m);
  qb_flush_events(kMainProgram, kRender);
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
