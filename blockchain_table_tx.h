#ifndef MYSQL_BLOCKCHAIN_TABLE_TX_H
#define MYSQL_BLOCKCHAIN_TABLE_TX_H

#include <memory>
#include <queue>
#include <vector>
#include <iomanip>
#include "types.h"
#include "connector.h"

#include <sql/sql_class.h>

/*
 * The tableScanData is also used during UPDATES and DELETES to find matching tuples.
 * We need to ensure that the tableScanData remains usable during one table scan even if
 * UPDATES and DELETES happen in place. For UPDATES this is not a problem, since
 * std::unordered_map is not re-hashed. But DELETES would invalid the iterator,
 * so they have to be deferred to the end of the table scan (rnd_end()).
 */

// Transaction table for one table!
class blockchain_table_tx {
 private:
  std::vector<PutOp> put_operations;
  std::vector<RemoveOp> remove_operations;
  std::queue<RemoveOp> pending_remove_operations;
  TXID id;
  std::vector<std::thread> commitPrepareWorkers;
  std::mutex commitPrepareSuccessMtx;
  bool commitPrepareSuccess;
  int prepareImmediately;

  void applyPutOpToCache(PutOp& op);
  void applyRemoveOpToCache(RemoveOp& op);

 public:
  tx_cache_t tableScanData;
  bool tableScanDataFilled;
  bool pendingRemoveActivated;

  blockchain_table_tx(THD* thd, int hton_slot, int prepare_immediately);

  void addPut(PutOp putOp, Connector* connector);
  void addRemove(RemoveOp removeOp, bool pending, Connector* connector);
  std::vector<PutOp>* getPutOperations();
  std::vector<RemoveOp>* getRemoveOperations();
  void reapplyPendingOperations();
  void applyPendingRemoveOps(Connector* connector);
  TXID getID();
  bool waitForCommitPrepareWorkers();

};

#endif  // MYSQL_BLOCKCHAIN_TABLE_TX_H
