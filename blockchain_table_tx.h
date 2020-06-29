#ifndef MYSQL_BLOCKCHAIN_TABLE_TX_H
#define MYSQL_BLOCKCHAIN_TABLE_TX_H

#include <memory>
#include <vector>
#include "types.h"

// Transaction table for one table!
class blockchain_table_tx {
 private:
  std::vector<PutOp> put_operations;
  std::vector<RemoveOp> remove_operations;

 public:
  std::vector<ManagedByteData> tableScanData;
  bool tableScanDataDeleteFlag;

  void addPut(PutOp data);
  void addRemove(RemoveOp data);
  std::vector<PutOp>* getPutOperations();
  std::vector<RemoveOp>* getRemoveOperations();

};

#endif  // MYSQL_BLOCKCHAIN_TABLE_TX_H
