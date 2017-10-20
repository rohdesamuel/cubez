#ifndef PRIVATE_UNIVERSE__H
#define PRIVATE_UNIVERSE__H

#include "concurrentqueue.h"
#include "component.h"
#include "cubez.h"
#include "defs.h"
#include "table.h"
#include "thread_pool.h"
#include "stack_memory.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define LOG_VAR(var) std::cout << #var << " = " << var << std::endl

using moodycamel::ConcurrentQueue;

class Runner {
 public:
  enum class State {
    ERROR = -10,
    UNKNOWN = 0,
    INITIALIZED = 10,
    STARTED = 20,
    RUNNING = 30,
    LOOPING = 40,
    STOPPED = 50,
    WAITING = 60,
    PAUSED = 70,
  };

  class TransitionGuard {
   public:
    TransitionGuard(Runner* runner, std::vector<State>&& allowed, State next):
      runner_(runner),
      old_(runner->state_),
      next_(next) {
      std::vector<State> allowed_from{allowed};
      runner_->wait_until(allowed_from);
      runner_->transition(allowed_from, next);
    }
    ~TransitionGuard() {
      runner_->transition(next_, old_);
    }
   private:
    Runner* runner_;
    State old_;
    State next_;
  };

  Runner(): state_(State::STOPPED) {}

  // Wait until runner enters an allowed state.
  void wait_until(const std::vector<State>& allowed);
  void wait_until(std::vector<State>&& allowed);

  qbResult transition(State allowed, State next);
  qbResult transition(const std::vector<State>& allowed, State next);
  qbResult transition(std::vector<State>&& allowed, State next);
  qbResult assert_in_state(State allowed);
  qbResult assert_in_state(const std::vector<State>& allowed);
  qbResult assert_in_state(std::vector<State>&& allowed);

  State state() const {
    return state_;
  }

 private:
  std::mutex state_change_;
  volatile State state_;
};

class CollectionRegistry {
 public:
   CollectionRegistry(): id_(0) {}

  qbCollection alloc(qbId program, qbCollectionAttr attr) {
    qbId id = id_++;
    qbCollection ret = new_collection(id, program, attr);
    collections_[program][id] = ret;
    return ret;
  }

  qbResult share(qbCollection collection, qbId program) {
    auto& destination = collections_[program];

    if (destination.find(collection->id) == destination.end()) {
      destination[collection->id] = collection;
      return QB_OK;
    }
    return QB_ERROR_ALREADY_EXISTS;
  }

 private:
  qbCollection new_collection(qbId id, qbId program, qbCollectionAttr attr) {
    qbCollection c = (qbCollection)calloc(1, sizeof(qbCollection_));
    *(qbId*)(&c->id) = id;
    *(qbId*)(&c->program_id) = program;
    c->interface.insert = attr->insert;
    c->interface.by_id = attr->accessor.id;
    c->interface.by_handle = attr->accessor.handle;
    c->interface.by_offset = attr->accessor.offset;
    c->interface.collection = attr->collection;
    c->count = attr->count;
    c->keys = attr->keys;
    c->values = attr->values;
    return c;
  }

  std::atomic_long id_;
  std::unordered_map<qbId, std::unordered_map<qbId, qbCollection>> collections_;
};

class SystemImpl {
 private:
  qbSystem system_;
  std::vector<qbCollection> sources_;
  std::vector<qbCollection> sinks_;
  std::vector<qbCollectionInterface> source_interfaces_;
  std::vector<qbCollectionInterface> sink_interfaces_;

  qbComponentJoin join_;
  void* user_state_;

  std::vector<qbElement> elements_data_;
  std::vector<qbElement_> elements_;
  void* element_values_;

  qbRunCondition run_condition_;
  qbTransform transform_;
  qbCallback callback_;

 public:
  SystemImpl(const qbSystemAttr_& attr, qbSystem system) :
    system_(system), join_(attr.join), user_state_(attr.state),
    transform_(attr.transform),
    callback_(attr.callback) {
    for(auto* component : attr.sinks) {
      sinks_.push_back(component->impl);
      sink_interfaces_.push_back(component->impl->interface);
    }
    for(auto* component : attr.sources) {
      sources_.push_back(component->impl);
      source_interfaces_.push_back(component->impl->interface);
    }

    for(auto* source : sources_) {
      qbElement_ element;
      element.read_buffer = nullptr;
      element.user_buffer = nullptr;
      element.size = source->values.size;
      element.indexed_by = qbIndexedBy::QB_INDEXEDBY_KEY;

      elements_.push_back(element);
    }

    for (size_t i = 0; i < sources_.size(); ++i) {
      auto source = sources_[i];
      auto found = std::find(sinks_.begin(), sinks_.end(), source);
      if (found != sinks_.end()) {
        elements_[i].interface = (*found)->interface;
      }
      elements_data_.push_back(&elements_[i]);
    }
  }

  static SystemImpl* from_raw(qbSystem system) {
    return (SystemImpl*)(((char*)system) + sizeof(qbSystem_));
  }
  
  void run(void* event = nullptr) {
    size_t source_size = sources_.size();
    size_t sink_size = sinks_.size();
    qbFrame frame;
    frame.event = event;
    frame.state = system_->user_state;

    if (!run_condition_ ||
        (run_condition_ && run_condition_(source_interfaces_.data(),
                                          sink_interfaces_.data(), &frame))) {
      if (transform_) {
        if (source_size == 0 && sink_size == 0) {
          run_0_to_0(&frame);
        } else if (source_size == 1 && sink_size == 0) {
          run_1_to_0(&frame);
        } else if (source_size == 1 && sink_size > 0) {
          run_1_to_n(&frame);
        } else if (source_size == 0 && sink_size > 0) {
          run_0_to_n(&frame);
        } else if (source_size == 1 && sink_size > 0) {
          run_1_to_n(&frame);
        } else if (source_size > 1) {
          run_m_to_n(&frame);
        }
      }

      if (callback_) {
        callback_(sink_interfaces_.data(), &frame);
      }
    }
  }

  void run_0_to_0(qbFrame* f) {
    transform_(elements_data_.data(), sink_interfaces_.data(), f);
  }

  void copy_to_element(void* k, void* v, qbOffset offset, qbElement element) {
    element->id = *(qbId*)k;
    element->read_buffer = v;
    element->offset = offset;
    element->indexed_by = qbIndexedBy::QB_INDEXEDBY_OFFSET;
  }

  void copy_to_element(void* k, void* v, qbElement element) {
    element->id = *(qbId*)k;
    element->read_buffer = v;
    element->indexed_by = qbIndexedBy::QB_INDEXEDBY_KEY;
  }

  void run_1_to_0(qbFrame* f) {
    qbCollection source = sources_[0];
    uint64_t count = source->count(&source->interface);
    uint8_t* keys = source->keys.data(&source->interface);
    uint8_t* values = source->values.data(&source->interface);

    for(uint64_t i = 0; i < count; ++i) {
      copy_to_element(
          (void*)(keys + source->keys.offset + i * source->keys.stride),
          (void*)(values + source->values.offset + i * source->values.stride),
          i, &elements_.front());
      transform_(elements_data_.data(), nullptr, f);
    }
  }

  void run_0_to_n(qbFrame* f) {
    transform_(nullptr, sink_interfaces_.data(), f);
  }

  void run_1_to_n(qbFrame* f) {
    qbCollection source = sources_[0];
    const uint64_t count = source->count(&source->interface);
    const uint8_t* keys = source->keys.data(&source->interface);
    const uint8_t* values = source->values.data(&source->interface);
    for(uint64_t i = 0; i < count; ++i) {
      copy_to_element(
          (void*)(keys + source->keys.offset + i * source->keys.stride),
          (void*)(values + source->values.offset + i * source->values.stride),
          i, &elements_.front());
      transform_(elements_data_.data(), sink_interfaces_.data(), f);
    }
  }

  void run_m_to_n(qbFrame* f) {
    qbCollection source = sources_[0];
    switch(join_) {
      case qbComponentJoin::QB_JOIN_INNER: {
        uint64_t min = 0xFFFFFFFFFFFFFFFF;
        for (size_t i = 0; i < sources_.size(); ++i) {
          qbCollection src = sources_[i];
          uint64_t maybe_min = src->count(&src->interface);
          if (maybe_min < min) {
            min = maybe_min;
            source = src;
          }
        }
      } break;
      case qbComponentJoin::QB_JOIN_LEFT:
        source = sources_[0];
      break;
      case qbComponentJoin::QB_JOIN_CROSS: {
        uint64_t* max_counts =
            (uint64_t*)alloca(sizeof(uint64_t) * sources_.size());
        uint64_t* indices =
            (uint64_t*)alloca(sizeof(uint64_t) * sources_.size());
        uint8_t** keys = (uint8_t**)alloca(sizeof(uint8_t*) * sources_.size());
        uint8_t** values = (uint8_t**)alloca(sizeof(uint8_t*) * sources_.size());

        memset(indices, 0, sizeof(uint64_t) * sources_.size());
        uint32_t indices_to_inc = 1;
        uint32_t max_index = 1 << sources_.size();
        for (size_t i = 0; i < sources_.size(); ++i) {
          keys[i] = sources_[i]->keys.data(&sources_[i]->interface);
          values[i] = sources_[i]->values.data(&sources_[i]->interface);
          max_counts[i] = sources_[i]->count(&sources_[i]->interface);
          indices[i] = 0;
        }

        while (indices_to_inc < max_index) {
          bool reached_max = false;
          size_t max_src = 0;
          for (size_t i = 0; i < sources_.size(); ++i) {
            reached_max |= indices[i] == max_counts[i];
            if (indices[i] == max_counts[i]) {
              max_src = i;
              continue;
            }

            qbCollection src = sources_[i];
            uint64_t index = indices[i];
            void* k =
                (void*)(keys[i] + src->keys.offset + index * src->keys.stride);
            void* v =
                (void*)(values[i] + src->values.offset + index * src->values.stride);
            copy_to_element(k, v, index, &elements_[i]);

            indices[i] += (1 << i) & indices_to_inc ? 1 : 0;
          }

          if (reached_max) {
            ++indices_to_inc;
            indices[max_src] = 0;
          }
          transform_(elements_data_.data(), sink_interfaces_.data(), f);
        }
      } return;
    }

    const uint64_t count = source->count(&source->interface);
    const uint8_t* keys = source->keys.data(&source->interface);
    switch(join_) {
      case qbComponentJoin::QB_JOIN_LEFT:
      case qbComponentJoin::QB_JOIN_INNER: {
        for(uint64_t i = 0; i < count; ++i) {
          void* k =
              (void*)(keys + source->keys.offset + i * source->keys.stride);

          bool should_continue = false;
          for (size_t j = 0; j < sources_.size(); ++j) {
            qbCollection c = sources_[j];
            void* v = c->interface.by_id(&c->interface, *(qbId*)k);
            if (!v) {
              should_continue = true;
              break;
            }
            copy_to_element(k, v, &elements_[j]);
          }
          if (should_continue) continue;

          transform_(elements_data_.data(), sink_interfaces_.data(), f);
        }
      } break;
      default:
        break;
    }
  }
};

class Channel {
 public:
  Channel(size_t size = 1) : size_(size) {
    // Start count is arbitrarily choosen.
    for (int i = 0; i < 16; ++i) {
      free_mem_.enqueue(alloc_message(nullptr));
    }
  }

  // Thread-safe.
  void* alloc_message(void* initial_val) {
    void* ret = nullptr;
    if (!free_mem_.try_dequeue(ret)) {
      ret = new_mem();
    }
    DEBUG_ASSERT(ret, QB_ERROR_NULL_POINTER);
    if (initial_val) {
      memmove(ret, initial_val, size_);
    }
    return ret;
  }

  qbResult send_message(void* message) {
    message_queue_.enqueue(alloc_message(message));
    return qbResult::QB_OK;
  }

  qbResult send_message_sync(void* message) {
    for (const auto& handler : handlers_) {
      (SystemImpl::from_raw(handler))->run(message);
    }

    free_message(message);
    return qbResult::QB_OK;
  }

  void add_handler(qbSystem s) {
    handlers_.insert(s);
  }

  void remove_handler(qbSystem s) {
    std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
    handlers_.erase(s);
  }

  void flush() {
    std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
    void* msg = nullptr;
    int max_events = message_queue_.size_approx();
    for (int i = 0; i < max_events; ++i) {
      if (!message_queue_.try_dequeue(msg)) {
        break;
      }

      for (const auto& handler : handlers_) {
        SystemImpl::from_raw(handler)->run(msg);
      }

      free_message(msg);
    }
  }

 private:
  // Thread-safe.
  void free_message(void* message) {
    free_mem_.enqueue(message);
  }

  void* new_mem() {
    return calloc(1, size_);
  }

  std::mutex handlers_mu_;
  std::set<qbSystem> handlers_;
  moodycamel::ConcurrentQueue<void*> message_queue_;
  moodycamel::ConcurrentQueue<void*> free_mem_;
  size_t size_;
};

class EventRegistry {
 public:
  EventRegistry(qbId program): program_(program), event_id_(0) { }

  // Thread-safe.
  qbResult create_event(qbEvent* event, qbEventAttr attr) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    qbId event_id = event_id_++;
    events_[event_id] = new Channel(attr->message_size);
    alloc_event(event_id, event, events_[event_id]);
    return qbResult::QB_OK;
  }

  // Thread-safe.
  void subscribe(qbEvent event, qbSystem system) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    find_event(event)->add_handler(system);
  }

  // Thread-safe.
  void unsubscribe(qbEvent event, qbSystem system) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    find_event(event)->remove_handler(system);
  }

  void flush_all() {
    for (auto event : events_) {
      event.second->flush();
    }
  }

  void flush(qbEvent event) {
    Channel* queue = find_event(event);
    if (queue) {
      queue->flush();
    }
  }

 private:
  void alloc_event(qbId id, qbEvent* event, Channel* channel) {
    *event = (qbEvent)calloc(1, sizeof(qbEvent_));
    *(qbId*)(&(*event)->id) = id;
    *(qbId*)(&(*event)->program) = program_;
    (*event)->channel = channel;
  }

  // Requires state_mutex_.
  Channel* find_event(qbEvent event) {
    auto it = events_.find(event->id);
    if (it != events_.end()) {
      return it->second;
    }
    return nullptr;
  }

  qbId program_;
  qbId event_id_;
  std::mutex state_mutex_;
  std::unordered_map<qbId, Channel*> events_; 
};

/*
 *  Sent to:
 *    Subscribed qbSystems in a qbProgram
 *    qbProgram/ qbSystem (to change running state)
 *  Effect:
 *    None (read-only)
 *    Change qbProgram/ qbSystem state
 *  Lifetime:
 *    Destroyed when read once
 *    Destroyed after qbSystems loop
 *  Callback:
 *    Yes
 *    No
 *
 *  qbTrigger:
 *    Sent To: qbProgram or qbSystem
 *    Effect: Change running state
 *    Lifetime: Destroyed when read once
 *    Callback: Available
 *
 *  Event:
 *    Sent To: qbProgram or subscribed qbSystems
 *    Effect: Change (non-running) state
 *    Lifetime: Destroyed after qbSystems loop
 *    Callback: Yes
 *    
 *  qbMessage:
 *    Sent To: qbProgram or subscribed qbSystems
 *    Effect: None (read-only)
 *    Lifetime: Destroyed after qbSystems loop
 *    Callback: No
 */

class ProgramImpl {
 private:
   typedef cubez::Table<qbCollection, std::set<qbSystem>> Mutators;
 public:
  ProgramImpl(qbProgram* program)
      : program_(program),
        events_(program->id) {}

  qbSystem create_system(const qbSystemAttr_& attr) {
    qbSystem system = new_system(systems_.size(), attr);
    systems_.push_back(system);

    enable_system(system);
    return system;
  }

  qbResult free_system(qbSystem system) {
    return disable_system(system);
  }

  qbResult enable_system(qbSystem system) {
    disable_system(system);

    if (system->policy.trigger == qbTrigger::LOOP) {
      loop_systems_.push_back(system);
      std::sort(loop_systems_.begin(), loop_systems_.end());
    } else if (system->policy.trigger == qbTrigger::EVENT) {
      event_systems_.insert(system);
    } else {
      return QB_ERROR_UNKNOWN_TRIGGER_POLICY;
    }

    return QB_OK;
  }

  qbResult disable_system(qbSystem system) {
    auto found = std::find(loop_systems_.begin(), loop_systems_.end(), system);
    if (found != loop_systems_.end()) {
      loop_systems_.erase(found);
    } else {
      event_systems_.erase(system);
    }
    return QB_OK;
  }

  bool contains_system(qbSystem system) {
    return std::find(systems_.begin(), systems_.end(), system) != systems_.end();
  }

  qbResult create_event(qbEvent* event, qbEventAttr attr) {
    return events_.create_event(event, attr);
  }

  void flush_events(qbEvent event) {
    events_.flush(event);
  }

  void flush_all_events() {
    events_.flush_all();
  }

  void subscribe_to(qbEvent event, qbSystem system) {
    events_.subscribe(event, system);
  }

  void unsubscribe_from(qbEvent event, qbSystem system) {
    events_.unsubscribe(event, system);
  }

  void run() {
    events_.flush_all();
    for(qbSystem p : loop_systems_) {
      SystemImpl::from_raw(p)->run();
    }
  }

 private:
  qbSystem new_system(qbId id, const qbSystemAttr_& attr) {
    qbSystem p = (qbSystem)calloc(1, sizeof(qbSystem_) + sizeof(SystemImpl));
    *(qbId*)(&p->id) = id;
    *(qbId*)(&p->program) = program_->id;
    p->policy.trigger = attr.trigger;
    p->policy.priority = attr.priority;
    p->user_state = attr.state;

    SystemImpl* impl = SystemImpl::from_raw(p);
    new (impl) SystemImpl(attr, p);

    return p;
  }

  qbResult add_system_to_collection(
      qbSystem system, qbCollection collection, Mutators* table) {
    if (!system || !collection || !table) {
      return QB_ERROR_NULL_POINTER;
    }

    if (!contains_system(system)) {
      return QB_ERROR_DOES_NOT_EXIST;
    }

    qbHandle handle = table->find(collection);
    if (handle == -1) {
      handle = table->insert(collection, {});
    }

    std::set<qbSystem>& systems = (*table)[handle];

    if (systems.find(system) == systems.end()) {
      systems.insert(system);
    } else {
      return QB_ERROR_ALREADY_EXISTS;
    }

    return QB_OK;
  }

  qbProgram* program_;
  Mutators writers_;
  Mutators readers_;

  EventRegistry events_;
  std::vector<qbSystem> systems_;
  std::vector<qbSystem> loop_systems_;
  std::set<qbSystem> event_systems_;
};

class ProgramThread {
 public:
  ProgramThread(qbProgram* program) :
      program_(program), is_running_(false) {} 

  ~ProgramThread() {
    release();
  }

  void run() {
    is_running_ = true;
    thread_.reset(new std::thread([this]() {
      while(is_running_) {
        ((ProgramImpl*)program_->self)->run();
      }
    }));
  }
  
  qbProgram* release() {
    if (is_running_) {
      is_running_ = false;
      thread_->join();
      thread_.reset();
    }
    return program_;
  }

 private:
  qbProgram* program_;
  std::unique_ptr<std::thread> thread_;
  std::atomic_bool is_running_;
};

class ProgramRegistry {
 public:
  ProgramRegistry() :
      program_threads_(std::thread::hardware_concurrency()) {}

  qbId create_program(const char* program) {
    std::hash<std::string> hasher;
    qbId id = hasher(program);

    DEBUG_OP(
      auto it = programs_.find(id);
      if (it != programs_.end()) {
        return -1;
      }
    );

    qbProgram* p = new_program(id, program);
    programs_[id] = p;
    return id;
  }

  qbResult detach_program(qbId program) {
    qbProgram* to_detach = get_program(program);
    std::unique_ptr<ProgramThread> program_thread(
        new ProgramThread(to_detach));
    program_thread->run();
    detached_[program] = std::move(program_thread);
    programs_.erase(programs_.find(program));
    return QB_OK;
  }

  qbResult join_program(qbId program) {
    programs_[program] = detached_.find(program)->second->release();
    detached_.erase(detached_.find(program));
    return QB_OK;
  }

  qbProgram* get_program(const char* program) {
    std::hash<std::string> hasher;
    qbId id = hasher(std::string(program));
    if (id == -1) {
      return nullptr;
    }
    return programs_[id];
  }

  qbProgram* get_program(qbId id) {
    return programs_[id];
  }

  inline ProgramImpl* to_impl(qbProgram* p) {
    return (ProgramImpl*)(p->self);
  }

  void run() {
    if (programs_.size() == 1) {
      for (auto& program : programs_) {
        ((ProgramImpl*)program.second->self)->run();
      }
    } else {
      std::vector<std::future<void>> programs;

      for (auto& program : programs_) {
        ProgramImpl* p = (ProgramImpl*)program.second->self;
        programs.push_back(program_threads_.enqueue(
                [p]() { p->run(); }));
      }

      for (auto& p : programs) {
        p.wait();
      }
    }
  }

  qbResult run_program(qbId program) {
    ProgramImpl* p = (ProgramImpl*)programs_[program]->self;
    p->run();
    return QB_OK;
  }

 private:
  qbProgram* new_program(qbId id, const char* name) {
    qbProgram* p = (qbProgram*)calloc(1, sizeof(qbProgram));
    *(qbId*)(&p->id) = id;
    *(char**)(&p->name) = strdup(name);
    p->self = new ProgramImpl{p};

    return p;
  }

  std::unordered_map<size_t, qbProgram*> programs_;
  std::unordered_map<size_t, std::unique_ptr<ProgramThread>> detached_;
  ThreadPool program_threads_;
};

struct ComponentInstance {
  qbId component_id;
  qbHandle instance_handle;
};

class ComponentRegistry {
 public:
  ComponentRegistry() : id_(0) {}

  qbResult create_component(qbComponent* component, qbComponentAttr attr) {
    new_component(component, attr);
    components_[(*component)->id] = *component;
    return qbResult::QB_OK;
  }

  static qbHandle create_component_instance(
      qbComponent component, qbId entity_id, const void* value) {
    return component->impl->interface.insert(&component->impl->interface,
                                             &entity_id, (void*)value);
  }

 private:
  qbResult new_component(qbComponent* component, qbComponentAttr attr) {
    *component = (qbComponent)calloc(1, sizeof(qbComponent_));
    *(qbId*)(&(*component)->id) = id_++;
    if (attr->impl) {
      (*component)->impl = attr->impl;
    } else {
      (*component)->impl = Component::new_collection(attr->program,
                                                     attr->data_size);
    }
    (*component)->data_size = attr->data_size;
    return qbResult::QB_OK;
  }

  std::unordered_map<qbId, qbComponent> components_;
  std::atomic_long id_;
};

class EntityRegistry {
 public:
  EntityRegistry() : id_(0) {}

  qbResult create_entityasync(qbEntity* entity, const qbEntityAttr_& attr) {
    new_entity(entity);
    add_components(*entity, attr.component_list);

    return qbResult::QB_OK;
  }

  qbResult create_entity(qbEntity* entity, const qbEntityAttr_& attr) {
    new_entity(entity);
    add_components(*entity, attr.component_list);

    return qbResult::QB_OK;
  }

  void add_components(qbEntity entity,
                      const std::vector<qbComponentInstance_>& components) {
    std::vector<ComponentInstance> instances;
    for (const auto instance : components) {
      qbComponent component = instance.component;

      instances.push_back({
          component->id,
          ComponentRegistry::create_component_instance(component, entity->id,
                                                       instance.data)
        });
    }
    entities_[entity->id] = instances;
  }

 private:
  qbResult new_entity(qbEntity* entity) {
    *entity = (qbEntity)calloc(1, sizeof(qbEntity_));
    *(qbId*)(&(*entity)->id) = id_++;
    return qbResult::QB_OK;
  }

  std::unordered_map<qbId, std::vector<ComponentInstance>> entities_;
  std::atomic_long id_;
};

class PrivateUniverse {
 public:
  PrivateUniverse();
  ~PrivateUniverse();

  qbResult init();
  qbResult start();
  qbResult stop();
  qbResult loop();

  qbId create_program(const char* name);
  qbResult run_program(qbId program);
  qbResult detach_program(qbId program);
  qbResult join_program(qbId program);

  // qbSystem manipulation.
  qbResult system_create(qbSystem* system, const qbSystemAttr_& attr);

  qbResult alloc_system(const struct qbSystemCreateInfo* create_info,
                        qbSystem* system);
  qbSystem copy_system(qbSystem system, const char* dest);
  qbResult free_system(qbSystem system);

  qbResult enable_system(qbSystem system);
  qbResult disable_system(qbSystem system);

  // qbCollection manipulation.
  qbResult collection_create(qbCollection* collection, qbCollectionAttr attr);
  qbResult collection_share(qbCollection collection, qbProgram destination);
  qbResult collection_copy(qbCollection collection, qbProgram destination);
  qbResult collection_destroy(qbCollection* collection);

  // Events.
  qbResult event_create(qbEvent* event, qbEventAttr attr);
  qbResult event_destroy(qbEvent* event);

  qbResult event_flush(qbEvent event);
  qbResult event_flushall(qbProgram event);

  qbResult event_subscribe(qbEvent event, qbSystem system);
  qbResult event_unsubscribe(qbEvent event, qbSystem system);

  qbResult event_send(qbEvent event, void* message);
  qbResult event_sendsync(qbEvent event, void* message);

  // Entity manipulation.
  qbResult entity_create(qbEntity* entity, const qbEntityAttr_& attr);

  // Component manipulation.
  qbResult component_create(qbComponent* component, qbComponentAttr attr);

 private:
  CollectionRegistry collections_;
  ProgramRegistry programs_;
  EntityRegistry entities_;
  ComponentRegistry components_;

  Runner runner_;
};

#endif  // PRIVATE_UNIVERSE__H
