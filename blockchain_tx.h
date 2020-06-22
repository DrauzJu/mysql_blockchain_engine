#ifndef MYSQL_BLOCKCHAIN_TX_H
#define MYSQL_BLOCKCHAIN_TX_H

#include <memory>
#include <vector>
#include "types.h"


class blockchain_tx {
 private:
  std::vector<std::unique_ptr<PutOp>> put_operations;
  std::vector<std::unique_ptr<RemoveOp>> remove_operations;

 public:
  void addPut(std::unique_ptr<PutOp> data);
  void addRemove(std::unique_ptr<RemoveOp> data);
  std::vector<std::unique_ptr<PutOp>>* getPutOpearations();
  std::vector<std::unique_ptr<RemoveOp>>* getRemoveOpearations();

};

#endif  // MYSQL_BLOCKCHAIN_TX_H
