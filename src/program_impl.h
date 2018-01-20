#ifndef PROGRAM_IMPL__H
#define PROGRAM_IMPL__H

#include "defs.h"
#include "component_registry.h"
#include "event_registry.h"

class ProgramImpl {
 public:
  ProgramImpl(qbProgram* program, ComponentRegistry* component_registry);
  static ProgramImpl* FromRaw(qbProgram* program);

  qbSystem CreateSystem(const qbSystemAttr_& attr);

  qbResult FreeSystem(qbSystem system);

  qbResult EnableSystem(qbSystem system);

  qbResult DisableSystem(qbSystem system);

  bool HasSystem(qbSystem system);

  qbResult CreateEvent(qbEvent* event, qbEventAttr attr);

  void FlushAllEvents();

  void SubscribeTo(qbEvent event, qbSystem system);

  void UnsubscribeFrom(qbEvent event, qbSystem system);

  void Ready();
  void Run();
  void Done();

  qbId Id();
  const char* Name();

 private:
  qbSystem AllocSystem(qbId id, const qbSystemAttr_& attr);

  qbProgram* program_;

  EventRegistry events_;
  ComponentRegistry* component_registry_;

  SparseMap<ComponentBuffer*> mutables_;
  std::vector<qbSystem> systems_;
  std::vector<qbSystem> loop_systems_;
  std::set<qbSystem> event_systems_;
  std::set<qbSystem> mutating_systems_;
};

#endif  // PROGRAM_IMPL__H

