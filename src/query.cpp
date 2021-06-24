#include "query.h"
#include "finally.h"

Query::Query(qbQuery query, GameState* state) : query_(query), state_(state) {}

qbResult Query::operator()(qbVar arg) {
  auto&& on_exit = make_finally([this]() {
    UnlockAll();
  });

  LockAll();
  if (query_->all_count > 0) {
    return DoQueryAll(arg);
  } else if (query_->any_count > 0) {
    return DoQueryAny(arg);
  } else if (query_->none_count > 0) {
    return DoQueryNone(arg);
  }

  query_->fn(qbInvalidEntity, query_->state, arg);
  return QB_OK;
}

void Query::LockAll() {
  for (size_t i = 0; i < query_->all_count; ++i) {
    Component* c = state_->ComponentGet(query_->all[i].component);
    c->Lock(query_->all[i].is_mutable == QB_TRUE);
  }

  for (size_t i = 0; i < query_->any_count; ++i) {
    Component* c = state_->ComponentGet(query_->any[i].component);
    c->Lock(query_->any[i].is_mutable == QB_TRUE);
  }
}

void Query::UnlockAll() {
  for (size_t i = 0; i < query_->all_count; ++i) {
    Component* c = state_->ComponentGet(query_->all[i].component);
    c->Unlock(query_->all[i].is_mutable == QB_TRUE);
  }

  for (size_t i = 0; i < query_->any_count; ++i) {
    Component* c = state_->ComponentGet(query_->any[i].component);
    c->Unlock(query_->any[i].is_mutable == QB_TRUE);
  }
}

qbResult Query::DoQueryAll(qbVar arg) {
  bool should_emit = true;
  Component* source = state_->ComponentGet(query_->all[0].component);
  for (auto id_data : *source) {
    for (size_t i = 0; i < query_->all_count; ++i) {
      qbQueryComponent_ qc = query_->all[i];
      should_emit &= state_->EntityHasComponent(id_data.first, qc.component);
    }

    if (!should_emit) {
      continue;
    }

    for (size_t i = 0; i < query_->none_count; ++i) {
      qbQueryComponent_ qc = query_->none[i];
      should_emit &= !state_->EntityHasComponent(id_data.first, qc.component);
    }

    if (!should_emit) {
      continue;
    }

    if (should_emit) {
      if (!query_->fn(id_data.first, query_->state, arg)) {
        return QB_OK;
      }
    }
  }
  return QB_OK;
}

qbResult Query::DoQueryAny(qbVar arg) {
  std::unordered_set<qbId> seen;
  for (size_t i = 0; i < query_->any_count; ++i) {
    Component* c_outer = state_->ComponentGet(query_->any[i].component);
    for (auto id_component : *c_outer) {
      qbId entity_id = id_component.first;
      if (seen.find(entity_id) != seen.end()) {
        continue;
      }

      bool should_emit = true;
      for (size_t i = 0; i < query_->none_count; ++i) {
        qbQueryComponent_ qc = query_->none[i];
        should_emit &= !state_->EntityHasComponent(entity_id, qc.component);
      }

      if (!should_emit) {
        continue;
      }

      if (!query_->fn(entity_id, query_->state, arg)) {
        return QB_OK;
      }
    }
  }
  return QB_OK;
}

qbResult Query::DoQueryNone(qbVar arg) {
  for (auto entity_id : state_->Entities()) {
    bool should_emit = true;
    for (size_t i = 0; i < query_->none_count; ++i) {
      qbQueryComponent_ qc = query_->none[i];
      should_emit &= !state_->EntityHasComponent(entity_id, qc.component);
    }

    if (!should_emit) {
      continue;
    }

    if (!query_->fn(entity_id, query_->state, arg)) {
      return QB_OK;
    }
  }
  return QB_OK;
}