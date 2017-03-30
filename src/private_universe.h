#ifndef PRIVATE_UNIVERSE__H
#define PRIVATE_UNIVERSE__H

#include "cubez.h"
#include "table.h"
#include "stack_memory.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#define LOG_VAR(var) std::cout << #var << " = " << var << std::endl

namespace cubez {

const static char NAMESPACE_DELIMETER = '/';

class Runner {
 public:
  enum class State {
    ERROR = -1,
    UNKNOWN = 0,
    INITIALIZED,
    STARTED,
    RUNNING,
    LOOPING,
    STOPPED,
    WAITING,
  };

  Runner(): state_(State::STOPPED) {}

  template<class Func_>
  Status::Code execute_if_in_state(std::vector<State>&& allowed, State next, Func_ f) {
    Status::Code status = transition(std::move(allowed), next);
    if (status == Status::OK) {
      f();
    }
    return status;
  }

  Status::Code transition(State allowed, State next);
  Status::Code transition(std::vector<State>&& allowed, State next);
  Status::Code assert_in_state(State allowed);
  Status::Code assert_in_state(std::vector<State>&& allowed);

 private:
  State state_;
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
    Collection* c = (Collection*)malloc(sizeof(Collection));
    memset(c, 0, sizeof(Collection));
    *(Id*)(&c->id) = id;
    *(char**)(&c->name) = (char*)name;
    *(void**)(&c->self) = new CollectionImpl(c);
    return c;
  }

  Table<std::string, Collection*> collections_;
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
      if (std::find(sources_.begin(), sources_.end(), source) == sources_.end()) {
        sources_.push_back(source);
      }
    }
  }

  void add_sink(Collection* sink) {
    if (sink) {
      if (std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end()) {
        sinks_.push_back(sink);
      }
    }
  }

  void run() {
    size_t source_size = sources_.size();
    size_t sink_size = sinks_.size();
    if (source_size == 1 && sink_size == 1) {
      run_1_to_1();
    } else if (source_size == 1 && sink_size == 0) {
      run_1_to_0();
    }
    if (pipeline_->callback) {
      //Collections sources {source_size, sources_.data()};
      //Collections sinks {sink_size, sinks_.data()};
      //pipeline_->callback(pipeline_, sources, sinks);
    }
  }

  void run_1_to_0() {
    Collection* source = sources_[0];
    uint64_t count = source->count(source);
    uint8_t* keys = source->keys.data(source);
    uint8_t* values = source->values.data(source);

    for(uint64_t i = 0; i < count; ++i) {
      thread_local static Stack stack;
      source->copy(
          keys + source->keys.offset + i * source->keys.size,
          values + source->values.offset + i * source->values.size,
          i, &stack);
      pipeline_->transform(&stack);
      stack.clear();
    }
  }

  void run_1_to_1() {
    Collection* source = sources_[0];
    Collection* sink = sinks_[0];
    const uint64_t count = source->count(source);
    const uint8_t* keys = source->keys.data(source);
    const uint8_t* values = source->values.data(source);

//#pragma omp parallel for
    for(uint64_t i = 0; i < count; ++i) {
      thread_local static Stack stack;
      source->copy(
          keys + source->keys.offset + i * source->keys.size,
          values + source->values.offset + i * source->values.size,
          i, &stack);
      pipeline_->transform(&stack);
      sink->mutate(sink, (const Mutation*)stack.top());
      stack.clear();
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

class ProgramImpl {
 private:
   typedef Table<Collection*, std::set<Pipeline*>> Mutators;
 public:
  ProgramImpl(Program* program) : program_(program) {}

  Pipeline* add_pipeline(Collection* source, Collection* sink) {
    Pipeline* pipeline = new_pipeline(pipelines_.size());
    pipelines_.push_back(pipeline);

    add_source(pipeline, source);
    add_sink(pipeline, sink);

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
    auto found = std::lower_bound(loop_pipelines_.begin(), loop_pipelines_.end(), pipeline);
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

  void run() {
    runner_.execute_if_in_state(
        { Runner::State::STOPPED, Runner::State::WAITING },
        Runner::State::RUNNING,
    [this]() {
      for(Pipeline* p : loop_pipelines_) {
        ((PipelineImpl*)p->self)->run();
      }
    });

    runner_.transition({ Runner::State::WAITING }, Runner::State::WAITING);
  }

  void detach() {
    static std::thread* detached = new std::thread([this]() {
      while(1) {
        run();
      }
    });

    detached->detach();
  }

 private:
  Pipeline* new_pipeline(Id id) {
    Pipeline* p = (Pipeline*)malloc(sizeof(Pipeline));
    memset(p, 0, sizeof(Pipeline));
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
  Runner runner_;

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
      p->self = new ProgramImpl{p};
      programs_[id] = p;
    }
    return id;
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

 private:
  Program* new_program(Id id, const char* name) {
    Program* p = (Program*)malloc(sizeof(Program));
    memset(p, 0, sizeof(Program));
    *(Id*)(&p->id) = id;
    *(char**)(&p->name) = (char*)name;
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
  Id detach_program(const char* name);
  Id join_program(const char* name);

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

 private:
  CollectionRegistry collections_;
  ProgramRegistry programs_;

  Runner runner_;
};

}  // namespace cubez

#endif  // PRIVATE_UNIVERSE__H
