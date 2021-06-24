#ifndef CUBEZ_QUERY__H
#define CUBEZ_QUERY__H

#include <cubez/cubez.h>
#include "game_state.h"

class Query {
public:
  Query(qbQuery query, GameState* state);

  qbResult operator()(qbVar arg);

private:
  qbResult DoQueryAll(qbVar arg);
  qbResult DoQueryAny(qbVar arg);
  qbResult DoQueryNone(qbVar arg);

  void LockAll();
  void UnlockAll();

  // Unowned.
  qbQuery query_;
  GameState* state_;  
};


#endif // CUBEZ_QUERY__H
