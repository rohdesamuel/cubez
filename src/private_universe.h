#ifndef PRIVATE_UNIVERSE__H
#define PRIVATE_UNIVERSE__H

#include "defs.h"

#include "component_registry.h"
#include "entity_registry.h"
#include "program_registry.h"

#include <mutex>

#define LOG_VAR(var) std::cout << #var << " = " << var << std::endl

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

  qbSystem copy_system(qbSystem system, const char* dest);
  qbResult free_system(qbSystem system);

  qbResult enable_system(qbSystem system);
  qbResult disable_system(qbSystem system);


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
  qbResult entity_destroy(qbEntity entity);
  qbResult entity_find(qbEntity* entity, qbId entity_id);
  bool entity_hascomponent(qbEntity entity, qbComponent component);
  qbResult entity_addcomponent(qbEntity entity, qbComponent component,
                               void* instance_data);
  qbResult entity_removecomponent(qbEntity entity, qbComponent component);

  // Instance manipulation.
  qbResult instance_oncreate(qbComponent component,
                             qbInstanceOnCreate on_create);
  qbResult instance_ondestroy(qbComponent component,
                              qbInstanceOnDestroy on_destroy);
  bool instance_hascomponent(qbInstance instance, qbComponent component);
  qbResult instance_getcomponent(qbInstance instance, qbComponent component, void* pbuffer);
  qbResult instance_getconst(qbInstance instance, void* pbuffer);
  qbResult instance_getmutable(qbInstance instance, void* pbuffer);
  qbResult instance_find(qbComponent component, qbEntity entity, void* pbuffer);

  // Component manipulation.
  qbResult component_create(qbComponent* component, qbComponentAttr attr);
  size_t component_getcount(qbComponent component);

  // Synchronization methods.
  qbBarrier barrier_create();
  void barrier_destroy(qbBarrier barrier);

  // Scene methods.
  qbResult scene_create(qbScene* scene, const char* name);
  qbResult scene_destroy(qbScene* scene);
  qbScene scene_global();
  qbResult scene_set(qbScene scene);
  qbResult scene_reset();
  qbResult scene_activate(qbScene scene);

  // Current program id of running thread.
  static thread_local qbId program_id;

 private:
  GameState* Baseline() {
    return baseline_->state;
  }

  GameState* WorkingScene() {
    return working_->state;
  }

  Runner runner_;

  // Must be initialized first.
  std::unique_ptr<ProgramRegistry> programs_;
  std::unique_ptr<ComponentRegistry> components_;
  qbScene baseline_;
  qbScene active_;
  qbScene working_;

  std::vector<qbBarrier> barriers_;
};

#endif  // PRIVATE_UNIVERSE__H
