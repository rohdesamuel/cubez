#ifndef PRIVATE_UNIVERSE__H
#define PRIVATE_UNIVERSE__H

#include "concurrentqueue.h"
#include "cubez.h"
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

#define DECLARE_FRAME_ARENAS() \
    thread_local static uint8_t MUTATION_ARENA[MUTATION_ARENA_SIZE]; \
    thread_local static uint8_t ARGS_ARENA[MAX_ARG_COUNT * sizeof(qbArg)]; \
    thread_local static uint8_t STACK_ARENA[MAX_ARG_COUNT][MAX_ARG_SIZE]

#define DECLARE_FRAME(var) \
    DECLARE_FRAME_ARENAS(); \
    uint8_t VAR_ARENA [sizeof(qbFrame)] = {0}; \
    qbFrame* VAR_ARENA_PTR = (qbFrame*)(&VAR_ARENA); \
    qbFrame& var = *VAR_ARENA_PTR; \
    init_frame(&var, ARGS_ARENA, STACK_ARENA, MUTATION_ARENA);

using moodycamel::ConcurrentQueue;

constexpr uint32_t MAX_ARG_COUNT = 8;
constexpr uint32_t MAX_ARG_SIZE = 128;
constexpr uint32_t MUTATION_ARENA_SIZE = 512;

namespace {

void init_frame(qbFrame* frame, uint8_t* args_arena, uint8_t stack_arena[][MAX_ARG_SIZE],
                uint8_t* mutation_arena) {
  frame->args.arg = (qbArg*)args_arena;
  frame->args.count = 0;
  for (uint32_t i = 0; i < MAX_ARG_COUNT; ++i) {
    frame->args.arg[i].data = (void*)(stack_arena + MAX_ARG_SIZE * i);
    frame->args.arg[i].size = MAX_ARG_SIZE;
  }
  frame->mutation.mutate_by = qbMutateBy::UNKNOWN;
  *(void**)(&frame->mutation.element) = mutation_arena;
}

}  // namespace

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

 private:
  std::mutex state_change_;
  volatile State state_;
};

class CollectionImpl {
 private:
  qbCollection* collection_;

 public:
  CollectionImpl(qbCollection* collection) : collection_(collection) {}

  inline qbCollection* collection() {
    return collection_;
  }
};

class CollectionRegistry {
 public:
  qbCollection* alloc(qbId program, const char* collection) {
    auto& collections = collections_[program];

    qbCollection* ret = nullptr;
    if (collections.find(collection) == -1) {
      qbHandle id = collections.insert(collection, nullptr);
      ret = new_collection(id, collection);
      collections[id] = ret;
    }
    return ret;
  }

  qbResult share(qbId source_program, const char* source_collection,
                 qbId dest_program, const char* dest_collection) {
    auto& source = collections_[source_program];
    auto& destination = collections_[dest_program];

    qbHandle src = source.find(source_collection);
    qbHandle dst = destination.find(dest_collection);
    if (src != -1 && dst == -1) {
      destination.insert(dest_collection, source[src]);
      return QB_OK;
    }
    return QB_ERROR_ALREADY_EXISTS;
  }

  qbCollection* get(qbId program, const char* collection) {
    auto& collections = collections_[program];

    qbCollection* ret = nullptr;
    qbHandle id;
    if ((id = collections.find(collection)) != -1) {
      ret = collections[id];
    }
    return ret;
  }

  inline static CollectionImpl* to_impl(qbCollection* c) {
    return (CollectionImpl*)c->self;
  }

 private:
  qbCollection* new_collection(qbId id, const char* name) {
    qbCollection* c = (qbCollection*)calloc(1, sizeof(qbCollection));
    *(qbId*)(&c->id) = id;
    *(char**)(&c->name) = (char*)name;
    *(void**)(&c->self) = new CollectionImpl(c);
    return c;
  }

  std::unordered_map<qbId, cubez::Table<std::string, qbCollection*>> collections_;
};

class FrameImpl {
 public:
  FrameImpl(cubez::Stack* stack): stack_(stack) {}

  static FrameImpl* from_raw(qbFrame* f) { return (FrameImpl*)f->self; }

  static qbArg* get_arg(qbFrame* frame, const char* name) {
    FrameImpl* self = from_raw(frame);
    auto it = self->args_.find(name);
    if (it == self->args_.end()) {
      return nullptr;
    }
    return it->second;
  }

  static qbArg* new_arg(qbFrame* frame, const char* name, size_t size) {
    if (frame->args.count >= MAX_ARG_COUNT) {
      return nullptr;
    }

    qbArg* ret = get_arg(frame, name);
    if (ret) {
      return ret;
    }

    FrameImpl* self = from_raw(frame);
    qbArg* arg = &frame->args.arg[frame->args.count++];
    arg->data = self->stack_->alloc(size);
    arg->size = size;
    self->args_[name] = arg;
    return arg;
  }

  static qbArg* set_arg(qbFrame* frame, const char* name, void* data, size_t size) {
    qbArg* ret = new_arg(frame, name, size);
    if (ret) {
      memcpy(ret->data, data, size);
    }
    return ret;
  }

 private:
  cubez::Stack* stack_;
  std::unordered_map<const char*, qbArg*> args_;
};

class SystemImpl {
 private:
  qbSystem* system_;
  std::vector<qbCollection*> sources_;
  std::vector<qbCollection*> sinks_;

 public:
  SystemImpl(qbSystem* system) : system_(system) {}
  
  void add_source(qbCollection* source) {
    if (source) {
      sources_.push_back(source);
    }
  }

  void add_sink(qbCollection* sink) {
    if (sink) {
      if (std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end()) {
        sinks_.push_back(sink);
      }
    }
  }

  void run(qbFrame* frame) {
    size_t source_size = sources_.size();
    size_t sink_size = sinks_.size();
    if (system_->transform) {
      if (source_size == 0 && sink_size == 0) {
        run_0_to_0(frame);
      } else if (source_size == 1 && sink_size == 1) {
        run_1_to_1(frame);
      } else if (source_size == 1 && sink_size == 0) {
        run_1_to_0(frame);
      } else if (source_size == 0 && sink_size == 1) {
        run_0_to_1(frame);
      }
    }

    if (system_->callback) {
      qbCollections sources;
      sources.count = sources_.size();
      sources.collection = sources_.data();

      qbCollections sinks;
      sinks.count = sinks_.size();
      sinks.collection = sinks_.data();

      if (frame) {
        system_->callback(system_, frame, &sources, &sinks);
      } else {
        DECLARE_FRAME(static_frame);
        system_->callback(system_, &static_frame, &sources, &sinks);
      }
    }
  }

  void run_0_to_0(qbFrame* f) {
    if (f) {
      system_->transform(system_, f);
    } else {
      DECLARE_FRAME(frame);
      system_->transform(system_, &frame);
    }
  }

  void run_0_to_1(qbFrame* f) {
    qbCollection* sink = sinks_[0];
    if (f) {
      system_->transform(system_, f);
      sink->mutate(sink, &f->mutation);
    } else {
      DECLARE_FRAME(frame);
      system_->transform(system_, &frame);
      sink->mutate(sink, &frame.mutation);
    }
  }

  void run_1_to_0(qbFrame* f) {
    qbCollection* source = sources_[0];
    uint64_t count = source->count(source);
    uint8_t* keys = source->keys.data(source);
    uint8_t* values = source->values.data(source);

    if (f) {
      for(uint64_t i = 0; i < count; ++i) {
        source->copy(
            keys + source->keys.offset + i * source->keys.stride,
            values + source->values.offset + i * source->values.stride,
            i, f);
        system_->transform(system_, f);
      }
    } else {
      for(uint64_t i = 0; i < count; ++i) {
        DECLARE_FRAME(frame);

        source->copy(
            keys + source->keys.offset + i * source->keys.stride,
            values + source->values.offset + i * source->values.stride,
            i, &frame);
        system_->transform(system_, &frame);
      }
    }
  }

  void run_1_to_1(qbFrame* f) {
    qbCollection* source = sources_[0];
    qbCollection* sink = sinks_[0];
    const uint64_t count = source->count(source);
    const uint8_t* keys = source->keys.data(source);
    const uint8_t* values = source->values.data(source);

//#pragma omp parallel for
    if (f) {
      for(uint64_t i = 0; i < count; ++i) {
        source->copy(
            keys + source->keys.offset + i * source->keys.stride,
            values + source->values.offset + i * source->values.stride,
            i, f);
        system_->transform(system_, f);
        sink->mutate(sink, &f->mutation);
      }
    } else {
      for(uint64_t i = 0; i < count; ++i) {
        DECLARE_FRAME(frame);

        source->copy(
            keys + source->keys.offset + i * source->keys.stride,
            values + source->values.offset + i * source->values.stride,
            i, &frame);
        system_->transform(system_, &frame);
        sink->mutate(sink, &frame.mutation);
      }
    }
  }

  void run_m_to_1(qbFrame* f) {
    qbCollection* source = sources_[0];
    qbCollection* sink = sinks_[0];
    const uint64_t count = source->count(source);
    const uint8_t* keys = source->keys.data(source);
    const uint8_t* values = source->values.data(source);

    if (f) {
      for (uint64_t i = 0; i < count; ++i) {
        for (size_t j = 1; j < sources_.size(); ++j) {
          void* key = alloca(source->values.stride);
          source->accessor.key(source, key); 
        }
        source->copy(
            keys + source->keys.offset + i * source->keys.stride,
            values + source->values.offset + i * source->values.stride,
            i, f);
        system_->transform(system_, f);
        sink->mutate(sink, &f->mutation);
      }
    } else {
      for (uint64_t i = 0; i < count; ++i) {
        DECLARE_FRAME(frame);

        source->copy(
            keys + source->keys.offset + i * source->keys.stride,
            values + source->values.offset + i * source->values.stride,
            i, &frame);
        system_->transform(system_, &frame);
        sink->mutate(sink, &frame.mutation);
      }
    }
  }
};

class MessageQueue {
 public:
  MessageQueue(size_t size = 1) : size_(size) {
    // Start count is arbitrarily choosen.
    for (int i = 0; i < 16; ++i) {
      free_mem_.enqueue(alloc_message(nullptr));
    }
  }

  // Thread-safe.
  qbMessage* alloc_message(qbChannel* c) {
    qbMessage* ret = nullptr;
    if (!free_mem_.try_dequeue(ret)) {
      ret = new_mem(c);
    }
    DEBUG_ASSERT(ret, QB_ERROR_NULL_POINTER);
    ret->channel = c;
    ret->size = size_;
    return ret;
  }

  // Thread-safe.
  void free_message(qbMessage* message) {
    free_mem_.enqueue(message);
  }

  void send_message(qbMessage* message) {
    message_queue_.enqueue(message);
  }

  void send_message_sync(qbMessage* message) {
    DECLARE_FRAME(frame);

    memcpy(&frame.message, message, sizeof(qbMessage));

    for (const auto& handler : handlers_) {
      ((SystemImpl*)(handler.second->self))->run(&frame);
    }

    free_message(message);
  }

  void add_handler(qbSystem* p) {
    handlers_[p->id] = p;
  }

  void remove_handler(qbId id) {
    std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
    handlers_.erase(id);
  }

  void flush() {
    std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
    qbMessage* msg = nullptr;
    int max_events = message_queue_.size_approx();
    for (int i = 0; i < max_events; ++i) {
      DECLARE_FRAME(frame);
      if (!message_queue_.try_dequeue(msg)) {
        break;
      }
      memcpy(&frame.message, msg, sizeof(qbMessage));

      for (const auto& handler : handlers_) {
        ((SystemImpl*)(handler.second->self))->run(&frame);
      }

      free_message(msg);
    }
  }

 private:
  qbMessage* new_mem(qbChannel* c) {
    qbMessage* ret = (qbMessage*)(malloc(sizeof(qbMessage) + size_));
    ret->channel = c;
    *(void**)(&ret->data) = (uint8_t*)ret + sizeof(qbMessage);
    ret->size = size_;
    return ret;
  }

  std::mutex handlers_mu_;
  std::unordered_map<qbId, qbSystem*> handlers_;
  moodycamel::ConcurrentQueue<qbMessage*> message_queue_;
  moodycamel::ConcurrentQueue<qbMessage*> free_mem_;
  size_t size_;
};

class ChannelImpl {
 public:
  ChannelImpl(qbChannel* self, MessageQueue* message_queue)
      : self_(self),
        message_queue_(message_queue) {}

  static ChannelImpl* to_impl(qbChannel* c) {
    return (ChannelImpl*)c->self;
  }

  // Thread-safe.
  qbMessage* alloc_message() {
    return message_queue_->alloc_message(self_);
  }

  // Thread-safe.
  void send_message(qbMessage* message) {
    message_queue_->send_message(message);
  }

  void send_message_sync(qbMessage* message) {
    message_queue_->send_message_sync(message);
  }

 private:
  qbChannel* self_;
  MessageQueue* message_queue_;
};

class EventRegistry {
 public:
  EventRegistry(qbId program): program_(program), event_id_(0) { }

  // Thread-safe.
  qbId create_event(const char* event, qbEventPolicy policy) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    qbId event_id;
    auto it = event_name_.find(event);
    if (it == event_name_.end()) {
      event_id = event_id_++;
      event_name_[event] = event_id;
    } else {
      event_id = it->second;
    }
    events_[event_id] = new MessageQueue(policy.size);
    return event_id;
  }

  // Thread-safe.
  qbSubscription* subscribe(const char* event, qbSystem* system) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    qbSubscription* ret = nullptr;
    qbId event_id = find_event(event);
    if (event_id != -1) {
      events_[event_id]->add_handler(system); 
      ret = new_subscription(event_id, system->id);
    }

    return ret;
  }

  // Thread-safe.
  void unsubscribe(qbSubscription* s) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    events_[s->event]->remove_handler(s->system);
  }

  // Thread-safe.
  qbChannel* open_channel(const char* event) {
    qbChannel* ret = nullptr;
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    qbId event_id = find_event(event);
    if (event_id != -1) {
      ret = (qbChannel*)malloc(sizeof(qbChannel));
      *(qbId*)(&ret->program) = program_;
      *(qbId*)(&ret->event) = event_id;
      ret->self = new ChannelImpl(ret, events_[event_id]);
    }

    return ret;
  }

  // Thread-safe.
  void close_channel(qbChannel* channel) {
    delete (ChannelImpl*)channel->self;
    free(channel);
  }

  void flush_all() {
    for (auto event : events_) {
      event.second->flush();
    }
  }

  void flush(const char* event) {
    qbId e = find_event(event);
    if (e == -1) {
      return;
    }

    events_[e]->flush();
  }

 private:
  // Requires state_mutex_.
  qbId find_event(const char* event) {
    qbId ret = -1;
    auto it = event_name_.find(event);
    if (it != event_name_.end()) {
      ret = it->second;
    }
    return ret;
  }

  qbSubscription* new_subscription(qbId event, qbId system) {
    qbSubscription* ret = (qbSubscription*)malloc(sizeof(qbSubscription));
    *(qbId*)(&ret->program) = program_;
    *(qbId*)(&ret->event) = event;
    *(qbId*)(&ret->system) = system;
    return ret;
  }

  void free_subscription(qbSubscription* subscription) {
    free(subscription);
  }

  qbId program_;
  qbId event_id_;
  std::mutex state_mutex_;
  std::unordered_map<std::string, qbId> event_name_;
  std::unordered_map<qbId, MessageQueue*> events_; 
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
   typedef cubez::Table<qbCollection*, std::set<qbSystem*>> Mutators;
 public:
  ProgramImpl(qbProgram* program)
      : program_(program),
        events_(program->id) {}

  qbSystem* alloc_system(qbCollection* source, qbCollection* sink) {
    qbSystem* system = new_system(systems_.size());
    systems_.push_back(system);

    if (source) {
      add_source(system, source);
    }

    if (sink) {
      add_sink(system, sink);
    }

    return system;
  }

  qbResult free_system(struct qbSystem* system) {
    return disable_system(system);
  }

  qbResult enable_system(struct qbSystem* system, qbExecutionPolicy policy) {
    disable_system(system);

    if (policy.trigger == qbTrigger::LOOP) {
      loop_systems_.push_back(system);
      std::sort(loop_systems_.begin(), loop_systems_.end());
    } else if (policy.trigger == qbTrigger::EVENT) {
      event_systems_.insert(system);
    } else {
      return QB_ERROR_UNKNOWN_TRIGGER_POLICY;
    }

    return QB_OK;
  }

  qbResult disable_system(struct qbSystem* system) {
    auto found = std::find(loop_systems_.begin(), loop_systems_.end(), system);
    if (found != loop_systems_.end()) {
      loop_systems_.erase(found);
    } else {
      event_systems_.erase(system);
    }
    return QB_OK;
  }

  qbResult add_source(qbSystem* system, qbCollection* source) {
    ((SystemImpl*)(system->self))->add_source(source);
    return add_system_to_collection(system, source, &readers_);
  }

  qbResult add_sink(qbSystem* system, qbCollection* sink) {
    ((SystemImpl*)(system->self))->add_sink(sink);
    return add_system_to_collection(system, sink, &writers_);
  }

  bool contains_system(qbSystem* system) {
    return std::find(systems_.begin(), systems_.end(), system) != systems_.end();
  }

  qbId create_event(const char* event, qbEventPolicy policy) {
    return events_.create_event(event, policy);
  }

  void flush_events(const char* event) {
    if (!event) {
      events_.flush_all();
    } else {
      events_.flush(event);
    }
  }

  qbChannel* open_channel(const char* event) {
    return events_.open_channel(event);
  }

  void close_channel(qbChannel* channel) {
    events_.close_channel(channel);
  }

  struct qbSubscription* subscribe_to(const char* event,
                                    struct qbSystem* system) {
    return events_.subscribe(event, system);
  }

  void unsubscribe_from(struct qbSubscription* subscription) {
    events_.unsubscribe(subscription);
  }

  void run() {
    events_.flush_all();
    for(qbSystem* p : loop_systems_) {
      ((SystemImpl*)p->self)->run(nullptr);
    }
  }

 private:
  qbSystem* new_system(qbId id) {
    qbSystem* p = (qbSystem*)calloc(1, sizeof(qbSystem));
    *(qbId*)(&p->id) = id;
    *(qbId*)(&p->program) = program_->id;
    p->self = new SystemImpl(p);
    return p;
  }

  qbResult add_system_to_collection(
      qbSystem* system, qbCollection* collection, Mutators* table) {
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

    std::set<qbSystem*>& systems = (*table)[handle];

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
  std::vector<qbSystem*> systems_;
  std::vector<qbSystem*> loop_systems_;
  std::set<qbSystem*> event_systems_;
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

  qbResult add_source(qbSystem* system, qbCollection* collection) {
    qbProgram* p = programs_[system->program];
    return to_impl(p)->add_source(system, collection);
  }

  qbResult add_sink(qbSystem* system, qbCollection* collection) {
    qbProgram* p = programs_[system->program];
    return to_impl(p)->add_sink(system, collection);
  }

  qbProgram* get_program(const char* program) {
    std::hash<std::string> hasher;
    qbId id = hasher(program);
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
    *(char**)(&p->name) = (char*)name;
    p->self = new ProgramImpl{p};
    return p;
  }

  std::unordered_map<size_t, qbProgram*> programs_;
  std::unordered_map<size_t, std::unique_ptr<ProgramThread>> detached_;
  ThreadPool program_threads_;
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
  struct qbSystem* alloc_system(const char* program, const char* source, const char* sink);
  struct qbSystem* copy_system(struct qbSystem* system, const char* dest);
  qbResult free_system(struct qbSystem* system);

  qbResult enable_system(struct qbSystem* system, qbExecutionPolicy policy);
  qbResult disable_system(struct qbSystem* system);

  // qbCollection manipulation.
  qbCollection* alloc_collection(const char* program, const char* collection);

  qbResult add_source(qbSystem*, const char* collection);
  qbResult add_sink(qbSystem*, const char* collection);

  qbResult share_collection(
      const char* source_program, const char* source_collection,
      const char* dest_program, const char* dest_collection);

  qbResult copy_collection(
      const char* source_program, const char* source_collection,
      const char* dest_program, const char* dest_collection);

  // Events.
  qbId create_event(const char* program, const char* event, qbEventPolicy policy);

  qbResult flush_events(const char* program, const char* event);

  struct qbChannel* open_channel(const char* program, const char* event);
  void close_channel(struct qbChannel* channel);

  struct qbSubscription* subscribe_to(
      const char* program, const char* event, struct qbSystem* system);
  void unsubscribe_from(struct qbSubscription* subscription);

 private:
  CollectionRegistry collections_;
  ProgramRegistry programs_;

  Runner runner_;
};

#endif  // PRIVATE_UNIVERSE__H
