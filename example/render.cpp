#include "render.h"

#include "constants.h"

#include <atomic>
#include <SDL2/SDL_opengl.h>

namespace render {

using cubez::Channel;
using cubez::Frame;
using cubez::Pipeline;
using cubez::send_message;
using cubez::open_channel;

// Event names
const char kInsert[] = "render_insert";


// Collections
Objects::Table* objects;
cubez::Collection* objects_collection;

// Channels
Channel* insert_channel;
Channel* render_channel;

// Pipelines
Pipeline* insert_pipeline;

// State
std::atomic_int next_id;

void initialize() {
  // Initialize collections.
  {
    std::cout << "Intializing render collections\n";
    objects = new Objects::Table();
    objects_collection = cubez::add_collection(kMainProgram, kCollection);
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

  // Initialize pipelines.
  {
    std::cout << "Intializing render pipelines\n";
    cubez::ExecutionPolicy policy;
    policy.priority = cubez::MIN_PRIORITY;
    policy.trigger = cubez::Trigger::EVENT;

    insert_pipeline = cubez::add_pipeline(kMainProgram, nullptr, kCollection);
    insert_pipeline->transform = [](Pipeline*, Frame* f) {
      std::cout << "insert render info\n";
      cubez::Mutation* mutation = &f->mutation;
      mutation->mutate_by = cubez::MutateBy::INSERT;
      Objects::Element* msg = (Objects::Element*)f->message.data;
      Objects::Element* el =
        (Objects::Element*)(mutation->element);
      *el = *msg;
    };
    cubez::enable_pipeline(insert_pipeline, policy);
    cubez::disable_pipeline(insert_pipeline);
  }

  // Initialize events.
  {
    std::cout << "Intializing render events\n";
    cubez::EventPolicy policy;
    policy.size = sizeof(Objects::Element);
    cubez::create_event(kMainProgram, kInsert, policy);
    cubez::subscribe_to(kMainProgram, kInsert, insert_pipeline);
    insert_channel = open_channel(kMainProgram, kInsert);
  }

  {
    cubez::EventPolicy policy;
    policy.size = sizeof(RenderEvent);
    cubez::create_event(kMainProgram, kRender, policy);
    render_channel = open_channel(kMainProgram, kRender);
  }

  std::cout << "Finished initializing render\n";
}

void present(RenderEvent* event) {
  cubez::Message* m = cubez::new_message(render_channel);
  *(RenderEvent*)m->data = *event;
  
  cubez::send_message(m);
  cubez::flush_events(kMainProgram, kRender);
}

cubez::Id create(Object* render_info) {
  cubez::Id new_id = next_id;
  ++next_id;

  cubez::Message* msg = cubez::new_message(insert_channel);
  Objects::Element el;
  el.indexed_by = cubez::IndexedBy::KEY;
  el.key = new_id;

  el.value = *render_info;
  *(Objects::Element*)msg->data = el;
  cubez::send_message(msg);

  return new_id;
}

}  // namespace render
