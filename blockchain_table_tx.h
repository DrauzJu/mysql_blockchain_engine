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
 * The table_scan_data is also used during UPDATES and DELETES to find matching tuples.
 * We need to ensure that the table_scan_data remains usable during one table scan even if
 * UPDATES and DELETES happen in place. For UPDATES this is not a problem, since
 * std::unordered_map is not re-hashed. But DELETES would invalid the iterator,
 * so they have to be deferred to the end of the table scan (rnd_end()).
 */

// Transaction table for one table!
class blockchain_table_tx {
 private:
  std::vector<Put_op> put_operations;
  std::vector<Remove_op> remove_operations;
  std::queue<Remove_op> pending_remove_operations;
  TXID id;
  std::vector<std::thread> commit_prepare_workers;
  std::mutex commit_prepare_success_mtx;
  bool commit_prepare_success;
  int prepare_immediately;

  void apply_put_op_to_cache(Put_op& op);
  void apply_remove_op_to_cache(Remove_op& op);
  std::string get_printable_id();

 public:
  tx_cache_t table_scan_data;
  bool table_scan_data_filled;
  bool pending_remove_activated;

  blockchain_table_tx(THD* thd, int hton_slot, int prepare_immediately);
  ~blockchain_table_tx();

  void add_put(Put_op putOp, Connector* connector);
  void add_remove(Remove_op removeOp, bool pending, Connector* connector);
  std::vector<Put_op>* get_put_operations();
  std::vector<Remove_op>* get_remove_operations();
  void reapply_pending_operations();
  void apply_pending_remove_ops(Connector* connector);
  TXID get_ID();
  bool wait_for_commit_prepare_workers();
  bool is_read_only();

};

#endif  // MYSQL_BLOCKCHAIN_TABLE_TX_H
