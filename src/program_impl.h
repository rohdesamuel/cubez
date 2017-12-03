#ifndef PROGRAM_IMPL__H
#define PROGRAM_IMPL__H

#include "defs.h"
#include "event_registry.h"

class ProgramImpl {
 public:
  ProgramImpl(qbProgram* program);
  static ProgramImpl* FromRaw(qbProgram* program);

  qbSystem CreateSystem(const qbSystemAttr_& attr);

  qbResult FreeSystem(qbSystem system);

  qbResult EnableSystem(qbSystem system);

  qbResult DisableSystem(qbSystem system);

  bool HasSystem(qbSystem system);

  qbResult CreateEvent(qbEvent* event, qbEventAttr attr);

  void FlushEvent(qbEvent event);

  void FlushAllEvents();

  void SubscribeTo(qbEvent event, qbSystem system);

  void UnsubscribeFrom(qbEvent event, qbSystem system);

  void Run();

 private:
  qbSystem AllocSystem(qbId id, const qbSystemAttr_& attr);

  qbProgram* program_;

  EventRegistry events_;
  std::vector<qbSystem> systems_;
  std::vector<qbSystem> loop_systems_;
  std::set<qbSystem> event_systems_;
};

#endif  // PROGRAM_IMPL__H

