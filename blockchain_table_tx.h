#ifndef MYSQL_BLOCKCHAIN_TABLE_TX_H
#define MYSQL_BLOCKCHAIN_TABLE_TX_H

// boost fix: https://github.com/boostorg/config/issues/322
#ifndef __clang_major__
#define __clang_major___WORKAROUND_GUARD 1
#else
#define __clang_major___WORKAROUND_GUARD 0
#endif
#define BOOST_FT_CC_IMPLICIT 0
#define BOOST_FT_CC_CDECL 0
#define BOOST_FT_CC_STDCALL 0
#define BOOST_FT_CC_PASCAL 0
#define BOOST_FT_CC_FASTCALL 0
#define BOOST_FT_CC_CLRCALL 0
#define BOOST_FT_CC_THISCALL 0
#define BOOST_FT_CC_IMPLICIT_THISCALL 0

#include <memory>
#include <queue>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "types.h"

/*
 * The tableScanData is also used during UPDATES and DELETES to find matching tuples.
 * We need to ensure that the tableScanData remains usable during one table scan even if
 * UPDATES and DELETES happen in place. For UPDATES this is not a problem, since
 * std::unordered_map is not re-hashed. But DELETES would invalid the iterator,
 * so they have to be deferred to the end of the table scan (rnd_end()).
 */

// todo: evaluate use of another data structure (like <tsl/hopscotch_map.h>)
// requirements: allows random access and access by key
using tx_cache_t = std::unordered_map<ManagedByteData, ManagedByteData>;

// Transaction table for one table!
class blockchain_table_tx {
 private:
  std::vector<PutOp> put_operations;
  std::vector<RemoveOp> remove_operations;
  std::queue<RemoveOp> pending_remove_operations;
  TXID id;

  void applyPutOpToCache(PutOp& op);
  void applyRemoveOpToCache(RemoveOp& op);

 public:
  tx_cache_t tableScanData;
  bool tableScanDataFilled;
  bool pendingRemoveActivated;

  blockchain_table_tx();

  void addPut(PutOp data);
  void addRemove(RemoveOp data, bool pending);
  std::vector<PutOp>* getPutOperations();
  std::vector<RemoveOp>* getRemoveOperations();
  void reapplyPendingOperations();
  void applyPendingRemoveOps();
  TXID getID();

};

#endif  // MYSQL_BLOCKCHAIN_TABLE_TX_H
