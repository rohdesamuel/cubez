#ifndef PRIVATE_UNIVERSE__H
#define PRIVATE_UNIVERSE__H

#include "cubez.h"
#include "table.h"
#include "stack_memory.h"
#include "concurrentqueue.h"

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

#define DECLARE_FRAME_ARENAS() \
    thread_local static uint8_t MUTATION_ARENA[MUTATION_ARENA_SIZE]; \
    thread_local static uint8_t ARGS_ARENA[MAX_ARG_COUNT * sizeof(Arg)]; \
    thread_local static uint8_t STACK_ARENA[MAX_ARG_COUNT][MAX_ARG_SIZE]

#define DECLARE_FRAME(var) \
    DECLARE_FRAME_ARENAS(); \
    Frame var; \
    init_frame(&var, ARGS_ARENA, STACK_ARENA, MUTATION_ARENA);

constexpr uint32_t MAX_ARG_COUNT = 8;
constexpr uint32_t MAX_ARG_SIZE = 128;
constexpr uint32_t MUTATION_ARENA_SIZE = 512;

namespace cubez {

namespace {

void init_frame(Frame* frame, uint8_t* args_arena, uint8_t stack_arena[][MAX_ARG_SIZE],
                uint8_t* mutation_arena) {
  frame->args.arg = (Arg*)args_arena;
  frame->args.count = 0;
  for (uint32_t i = 0; i < MAX_ARG_COUNT; ++i) {
    frame->args.arg[i].data = (void*)(stack_arena + MAX_ARG_SIZE * i);
    frame->args.arg[i].size = MAX_ARG_SIZE;
  }
  frame->mutation.mutate_by = MutateBy::UNKNOWN;
  frame->mutation.element = mutation_arena;
}

}  // namespace

const static char NAMESPACE_DELIMETER = '/';
const static uint8_t MAX_ARG_COUNT = 16;

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

  Status::Code transition(State allowed, State next);
  Status::Code transition(const std::vector<State>& allowed, State next);
  Status::Code transition(std::vector<State>&& allowed, State next);
  Status::Code assert_in_state(State allowed);
  Status::Code assert_in_state(const std::vector<State>& allowed);
  Status::Code assert_in_state(std::vector<State>&& allowed);

 private:
  std::mutex state_change_;
  volatile State state_;
};

class CollectionImpl {
 private:
  Collection* collection_;

 public:
  CollectionImpl(Collection* collection) : collection_(collection) {}

  inline Collection* collection() {
    return collection_;
  }
};

class CollectionRegistry {
 public:
  Collection* add(const char* program, const char* collection) {
    std::string name = std::string(program) + NAMESPACE_DELIMETER + std::string(collection);
    Collection* ret = nullptr;
    if (collections_.find(name) == -1) {
      Handle id = collections_.insert(name, nullptr);
      ret = new_collection(id, collection);
      collections_[id] = ret;
    }
    return ret;
  }

  Status::Code share(const char* source, const char* dest) {
    Handle src = collections_.find(source);
    Handle dst = collections_.find(dest);
    if (src != -1 && dst == -1) {
      collections_.insert(dest, collections_[src]);
      return Status::OK;
    }
    return Status::ALREADY_EXISTS;
  }

  Collection* get(const char* name) {
    Collection* ret = nullptr;
    Handle id;
    if ((id = collections_.find(name)) != -1) {
      ret = collections_[id];
    }
    return ret;
  }

  Collection* get(const char* program, const char* collection) {
    std::string name = std::string(program) + NAMESPACE_DELIMETER + std::string(collection);
    return get(name.data());
  }

  inline static CollectionImpl* to_impl(Collection* c) {
    return (CollectionImpl*)c->self;
  }

 private:
  Collection* new_collection(Id id, const char* name) {
    Collection* c = (Collection*)calloc(1, sizeof(Collection));
    *(Id*)(&c->id) = id;
    *(char**)(&c->name) = (char*)name;
    *(void**)(&c->self) = new CollectionImpl(c);
    return c;
  }

  Table<std::string, Collection*> collections_;
};

class FrameImpl {
 public:
  FrameImpl(Stack* stack): stack_(stack) {}

  static FrameImpl* from_raw(Frame* f) { return (FrameImpl*)f->self; }

  static Arg* get_arg(Frame* frame, const char* name) {
    FrameImpl* self = from_raw(frame);
    auto it = self->args_.find(name);
    if (it == self->args_.end()) {
      return nullptr;
    }
    return it->second;
  }

  static Arg* new_arg(Frame* frame, const char* name, size_t size) {
    if (frame->args.count >= MAX_ARG_COUNT) {
      return nullptr;
    }

    Arg* ret = get_arg(frame, name);
    if (ret) {
      return ret;
    }

    FrameImpl* self = from_raw(frame);
    Arg* arg = &frame->args.arg[frame->args.count++];
    arg->data = self->stack_->alloc(size);
    arg->size = size;
    self->args_[name] = arg;
    return arg;
  }

  static Arg* set_arg(Frame* frame, const char* name, void* data, size_t size) {
    Arg* ret = new_arg(frame, name, size);
    if (ret) {
      memcpy(ret->data, data, size);
    }
    return ret;
  }

 private:
  Stack* stack_;
  std::unordered_map<const char*, Arg*> args_;
};

class PipelineImpl {
 private:
  Pipeline* pipeline_;
  std::vector<Collection*> sources_;
  std::vector<Collection*> sinks_;

 public:
  PipelineImpl(Pipeline* pipeline) : pipeline_(pipeline) {}
  
  void add_source(Collection* source) {
    if (source) {
      sources_.push_back(source);
    }
  }

  void add_sink(Collection* sink) {
    if (sink) {
      if (std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end()) {
        sinks_.push_back(sink);
      }
    }
  }

  void run(Frame* frame) {
    size_t source_size = sources_.size();
    size_t sink_size = sinks_.size();
    if (pipeline_->transform) {
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

    if (pipeline_->callback) {
      Collections sources;
      sources.count = sources_.size();
      sources.collection = sources_.data();

      Collections sinks;
      sinks.count = sinks_.size();
      sinks.collection = sinks_.data();

      if (frame) {
        pipeline_->callback(pipeline_, frame, &sources, &sinks);
      } else {
        DECLARE_FRAME(static_frame);
        pipeline_->callback(pipeline_, &static_frame, &sources, &sinks);
      }
    }
  }

  void run_0_to_0(Frame* f) {
    if (f) {
      pipeline_->transform(pipeline_, f);
    } else {
      DECLARE_FRAME(frame);
      pipeline_->transform(pipeline_, &frame);
    }
  }

  void run_0_to_1(Frame* f) {
    Collection* sink = sinks_[0];
    if (f) {
      pipeline_->transform(pipeline_, f);
      sink->mutate(sink, &f->mutation);
    } else {
      DECLARE_FRAME(frame);
      pipeline_->transform(pipeline_, &frame);
      sink->mutate(sink, &frame.mutation);
    }
  }

  void run_1_to_0(Frame* f) {
    Collection* source = sources_[0];
    uint64_t count = source->count(source);
    uint8_t* keys = source->keys.data(source);
    uint8_t* values = source->values.data(source);

    if (f) {
      for(uint64_t i = 0; i < count; ++i) {
        source->copy(
            keys + source->keys.offset + i * source->keys.size,
            values + source->values.offset + i * source->values.size,
            i, f);
        pipeline_->transform(pipeline_, f);
      }
    } else {
      for(uint64_t i = 0; i < count; ++i) {
        DECLARE_FRAME(frame);

        source->copy(
            keys + source->keys.offset + i * source->keys.size,
            values + source->values.offset + i * source->values.size,
            i, &frame);
        pipeline_->transform(pipeline_, &frame);
      }
    }
  }

  void run_1_to_1(Frame* f) {
    Collection* source = sources_[0];
    Collection* sink = sinks_[0];
    const uint64_t count = source->count(source);
    const uint8_t* keys = source->keys.data(source);
    const uint8_t* values = source->values.data(source);

//#pragma omp parallel for
    if (f) {
      for(uint64_t i = 0; i < count; ++i) {
        source->copy(
            keys + source->keys.offset + i * source->keys.size,
            values + source->values.offset + i * source->values.size,
            i, f);
        pipeline_->transform(pipeline_, f);
        sink->mutate(sink, &f->mutation);
      }
    } else {
      for(uint64_t i = 0; i < count; ++i) {
        DECLARE_FRAME(frame);

        source->copy(
            keys + source->keys.offset + i * source->keys.size,
            values + source->values.offset + i * source->values.size,
            i, &frame);
        pipeline_->transform(pipeline_, &frame);
        sink->mutate(sink, &frame.mutation);
      }
    }
  }

  /*
  void run_m_to_n() {
    Stack stack;
    std::unordered_map<uint8_t*, std::vector<uint8_t*>> joined;

    // Naive Hash-Join implementation.
    uint64_t min_count = std::numeric_limits<uint64_t>::max();
    Collection* min_collection = nullptr;
    for (Collection* c : sources_) {
      if (c->count(c) < min_count) {
        min_collection = c;
      }
    }

    {
      uint8_t* keys = min_collection->keys.data + min_collection->keys.offset;
      uint8_t* values = min_collection->values.data + min_collection->values.offset;
      for(uint64_t i = 0; i < min_count; ++i) {
        joined[keys].push_back(values);

        keys += min_collection->keys.size;
        values += min_collection->values.size;
      }
    }

    for (Collection* c : sources_) {
      if (c == min_collection) continue;
      if (c == sources_.back()) continue;

      uint8_t* keys = min_collection->keys.data + min_collection->keys.offset;
      uint8_t* values = min_collection->values.data + min_collection->values.offset;
      for(uint64_t i = 0; i < min_count; ++i) {
        joined[keys].push_back(values);

        keys += c->keys.size;
        values += c->values.size;
      }
    }

    {
      Collection* c = sources_.back();
      uint8_t* keys = c->keys.data + c->keys.offset;
      uint8_t* values = c->values.data + c->values.offset;
      for(uint64_t i = 0; i < min_count; ++i) {
        auto joined_values = joined[keys];
        joined_values.push_back(values);

        if (joined_values.size() == sources_.size()) {
        }

        keys += c->keys.size;
        values += c->values.size;
      }
    }
  }
  */
};

class MessageQueue {
 public:
  MessageQueue(size_t size = 1) : size_(size) {
    // Start count is arbitrarily choosen.
    for (int i = 0; i < 16; ++i) {
      free_mem_.enqueue(new_message(nullptr));
    }
  }

  // Thread-safe.
  Message* new_message(Channel* c) {
    Message* ret = nullptr;
    if (!free_mem_.try_dequeue(ret)) {
      ret = new_mem(c);
    }
    DEBUG_ASSERT(ret, Status::Code::NULL_POINTER);
    ret->channel = c;
    ret->size = size_;
    return ret;
  }

  // Thread-safe.
  void free_message(Message* message) {
    free_mem_.enqueue(message);
  }

  void send_message(Message* message) {
    message_queue_.enqueue(message);
  }

  void add_handler(Pipeline* p) {
    handlers_[p->id] = p;
  }

  void remove_handler(Id id) {
    std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
    handlers_.erase(id);
  }

  void flush() {
    std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
    Message* msg = nullptr;
    int max_events = message_queue_.size_approx();
    for (int i = 0; i < max_events; ++i) {
      DECLARE_FRAME(frame);
      if (!message_queue_.try_dequeue(msg)) {
        break;
      }
      frame.message = *msg;

      for (const auto& handler : handlers_) {
        ((PipelineImpl*)(handler.second->self))->run(&frame);
      }

      free_message(msg);
    }
  }

 private:
  Message* new_mem(Channel* c) {
    Message* ret = (Message*)(malloc(sizeof(Message) + size_));
    ret->channel = c;
    ret->data = (uint8_t*)ret + sizeof(Message);
    ret->size = size_;
    return ret;
  }

  std::mutex handlers_mu_;
  std::unordered_map<Id, Pipeline*> handlers_;
  moodycamel::ConcurrentQueue<Message*> message_queue_;
  moodycamel::ConcurrentQueue<Message*> free_mem_;
  size_t size_;
};

class ChannelImpl {
 public:
  ChannelImpl(Channel* self, MessageQueue* message_queue)
      : self_(self),
        message_queue_(message_queue) {}

  static ChannelImpl* to_impl(Channel* c) {
    return (ChannelImpl*)c->self;
  }

  // Thread-safe.
  Message* new_message() {
    return message_queue_->new_message(self_);
  }

  // Thread-safe.
  void send_message(Message* message) {
    message_queue_->send_message(message);
  }

 private:
  Channel* self_;
  MessageQueue* message_queue_;
};

class EventRegistry {
 public:
  EventRegistry(Id program): program_(program), event_id_(0) { }

  // Thread-safe.
  Id create_event(const char* event, EventPolicy policy) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    Id event_id;
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
  Subscription* subscribe(const char* event, Pipeline* pipeline) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    Subscription* ret = nullptr;
    Id event_id = find_event(event);
    if (event_id != -1) {
      events_[event_id]->add_handler(pipeline); 
      ret = new_subscription(event_id, pipeline->id);
    }

    return ret;
  }

  // Thread-safe.
  void unsubscribe(Subscription* s) {
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    events_[s->event]->remove_handler(s->pipeline);
  }

  // Thread-safe.
  Channel* open_channel(const char* event) {
    Channel* ret = nullptr;
    std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
    Id event_id = find_event(event);
    if (event_id != -1) {
      ret = (Channel*)malloc(sizeof(Channel));
      *(Id*)(&ret->program) = program_;
      *(Id*)(&ret->event) = event_id;
      ret->self = new ChannelImpl(ret, events_[event_id]);
    }

    return ret;
  }

  // Thread-safe.
  void close_channel(Channel* channel) {
    delete (ChannelImpl*)channel->self;
    free(channel);
  }

  void flush_all() {
    for (auto event : events_) {
      event.second->flush();
    }
  }

 private:
  // Requires state_mutex_.
  Id find_event(const char* event) {
    Id ret = -1;
    auto it = event_name_.find(event);
    if (it != event_name_.end()) {
      ret = it->second;
    }
    return ret;
  }

  Subscription* new_subscription(Id event, Id pipeline) {
    Subscription* ret = (Subscription*)malloc(sizeof(Subscription));
    *(Id*)(&ret->program) = program_;
    *(Id*)(&ret->event) = event;
    *(Id*)(&ret->pipeline) = pipeline;
    return ret;
  }

  void free_subscription(Subscription* subscription) {
    free(subscription);
  }

  Id program_;
  Id event_id_;
  std::mutex state_mutex_;
  std::unordered_map<std::string, Id> event_name_;
  std::unordered_map<Id, MessageQueue*> events_; 
};

/*
 *  Sent to:
 *    Subscribed Pipelines in a Program
 *    Program/ Pipeline (to change running state)
 *  Effect:
 *    None (read-only)
 *    Change Program/ Pipeline state
 *  Lifetime:
 *    Destroyed when read once
 *    Destroyed after Pipelines loop
 *  Callback:
 *    Yes
 *    No
 *
 *  Trigger:
 *    Sent To: Program or Pipeline
 *    Effect: Change running state
 *    Lifetime: Destroyed when read once
 *    Callback: Available
 *
 *  Event:
 *    Sent To: Program or subscribed Pipelines
 *    Effect: Change (non-running) state
 *    Lifetime: Destroyed after Pipelines loop
 *    Callback: Yes
 *    
 *  Message:
 *    Sent To: Program or subscribed Pipelines
 *    Effect: None (read-only)
 *    Lifetime: Destroyed after Pipelines loop
 *    Callback: No
 */

class ProgramImpl {
 private:
   typedef Table<Collection*, std::set<Pipeline*>> Mutators;
 public:
  ProgramImpl(Program* program)
      : program_(program),
        events_(program->id) {}

  Pipeline* add_pipeline(Collection* source, Collection* sink) {
    Pipeline* pipeline = new_pipeline(pipelines_.size());
    pipelines_.push_back(pipeline);

    if (source) {
      add_source(pipeline, source);
    }

    if (sink) {
      add_sink(pipeline, sink);
    }

    return pipeline;
  }

  Status::Code remove_pipeline(struct Pipeline* pipeline) {
    return disable_pipeline(pipeline);
  }

  Status::Code enable_pipeline(struct Pipeline* pipeline, ExecutionPolicy policy) {
    disable_pipeline(pipeline);

    if (policy.trigger == Trigger::LOOP) {
      loop_pipelines_.push_back(pipeline);
      std::sort(loop_pipelines_.begin(), loop_pipelines_.end());
    } else if (policy.trigger == Trigger::EVENT) {
      event_pipelines_.insert(pipeline);
    } else {
      return Status::UNKNOWN_TRIGGER_POLICY;
    }

    return Status::OK;
  }

  Status::Code disable_pipeline(struct Pipeline* pipeline) {
    auto found = std::find(loop_pipelines_.begin(), loop_pipelines_.end(), pipeline);
    if (found != loop_pipelines_.end()) {
      loop_pipelines_.erase(found);
    } else {
      event_pipelines_.erase(pipeline);
    }
    return Status::OK;
  }

  Status::Code add_source(Pipeline* pipeline, Collection* source) {
    ((PipelineImpl*)(pipeline->self))->add_source(source);
    return add_pipeline_to_collection(pipeline, source, &readers_);
  }

  Status::Code add_sink(Pipeline* pipeline, Collection* sink) {
    ((PipelineImpl*)(pipeline->self))->add_sink(sink);
    return add_pipeline_to_collection(pipeline, sink, &writers_);
  }

  bool contains_pipeline(Pipeline* pipeline) {
    return std::find(pipelines_.begin(), pipelines_.end(), pipeline) != pipelines_.end();
  }

  Id create_event(const char* event, EventPolicy policy) {
    return events_.create_event(event, policy);
  }

  Channel* open_channel(const char* event) {
    return events_.open_channel(event);
  }

  void close_channel(Channel* channel) {
    events_.close_channel(channel);
  }

  struct Subscription* subscribe_to(const char* event,
                                    struct Pipeline* pipeline) {
    return events_.subscribe(event, pipeline);
  }

  void unsubscribe_from(struct Subscription* subscription) {
    events_.unsubscribe(subscription);
  }

  void run() {
    events_.flush_all();
    for(Pipeline* p : loop_pipelines_) {
      ((PipelineImpl*)p->self)->run(nullptr);
    }
  }

 private:
  Pipeline* new_pipeline(Id id) {
    Pipeline* p = (Pipeline*)calloc(1, sizeof(Pipeline));
    *(Id*)(&p->id) = id;
    *(Id*)(&p->program) = program_->id;
    p->self = new PipelineImpl(p);
    return p;
  }

  Status::Code add_pipeline_to_collection(
      Pipeline* pipeline, Collection* collection, Mutators* table) {
    if (!pipeline || !collection || !table) {
      return Status::NULL_POINTER;
    }

    if (!contains_pipeline(pipeline)) {
      return Status::DOES_NOT_EXIST;
    }

    Handle handle = table->find(collection);
    if (handle == -1) {
      handle = table->insert(collection, {});
    }

    std::set<Pipeline*>& pipelines = (*table)[handle];

    if (pipelines.find(pipeline) == pipelines.end()) {
      pipelines.insert(pipeline);
    } else {
      return Status::ALREADY_EXISTS;
    }

    return Status::OK;
  }

  Program* program_;
  Mutators writers_;
  Mutators readers_;

  EventRegistry events_;
  std::vector<Pipeline*> pipelines_;
  std::vector<Pipeline*> loop_pipelines_;
  std::set<Pipeline*> event_pipelines_;
};

class ProgramRegistry {
 public:
  Id create_program(const char* program) {
    Id id = programs_.find(program);
    if (id == -1) {
      id = programs_.insert(program, nullptr);
      Program* p = new_program(id, program);
      programs_[id] = p;
    }
    return id;
  }

  Id detach_program(const char*) {
    return -1;
  }

  Id join_program(const char*) {
    return -1;
  }

  Status::Code add_source(Pipeline* pipeline, Collection* collection) {
    Program* p = programs_[pipeline->program];
    return to_impl(p)->add_source(pipeline, collection);
  }

  Status::Code add_sink(Pipeline* pipeline, Collection* collection) {
    Program* p = programs_[pipeline->program];
    return to_impl(p)->add_sink(pipeline, collection);
  }

  Program* get_program(const char* program) {
    Handle id = programs_.find(program);
    if (id == -1) {
      return nullptr;
    }
    return programs_[id];
  }

  Program* get_program(Id id) {
    return programs_[id];
  }

  inline ProgramImpl* to_impl(Program* p) {
    return (ProgramImpl*)(p->self);
  }

  void run() {
    for (uint64_t i = 0; i < programs_.size(); ++i) {
      ProgramImpl* p = (ProgramImpl*)programs_.values[i]->self;
      p->run();
    }
  }

  Status::Code run_program(Id program) {
    ProgramImpl* p = (ProgramImpl*)programs_[program]->self;
    p->run();
    return Status::Code::OK;
  }

 private:
  Program* new_program(Id id, const char* name) {
    Program* p = (Program*)calloc(1, sizeof(Program));
    *(Id*)(&p->id) = id;
    *(char**)(&p->name) = (char*)name;
    p->self = new ProgramImpl{p};
    return p;
  }

  Table<std::string, Program*> programs_;
};

class PrivateUniverse {
 public:
  PrivateUniverse();
  ~PrivateUniverse();

  Status::Code init();
  Status::Code start();
  Status::Code stop();
  Status::Code loop();

  Id create_program(const char* name);
  Status::Code run_program(Id program);

  // Pipeline manipulation.
  struct Pipeline* add_pipeline(const char* program, const char* source, const char* sink);
  struct Pipeline* copy_pipeline(struct Pipeline* pipeline, const char* dest);
  Status::Code remove_pipeline(struct Pipeline* pipeline);

  Status::Code enable_pipeline(struct Pipeline* pipeline, ExecutionPolicy policy);
  Status::Code disable_pipeline(struct Pipeline* pipeline);

  // Collection manipulation.
  Collection* add_collection(const char* program, const char* collection);

  Status::Code add_source(Pipeline*, const char* collection);
  Status::Code add_sink(Pipeline*, const char* collection);

  Status::Code share_collection(const char* source, const char* dest);
  Status::Code copy_collection(const char* source, const char* dest);

  // Events.
  Id create_event(const char* program, const char* event, EventPolicy policy);

  struct Channel* open_channel(const char* program, const char* event);
  void close_channel(struct Channel* channel);

  struct Subscription* subscribe_to(
      const char* program, const char* event, struct Pipeline* pipeline);
  void unsubscribe_from(struct Subscription* subscription);

 private:
  CollectionRegistry collections_;
  ProgramRegistry programs_;

  Runner runner_;
};

}  // namespace cubez

#endif  // PRIVATE_UNIVERSE__H
