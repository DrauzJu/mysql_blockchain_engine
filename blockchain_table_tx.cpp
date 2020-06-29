#include "blockchain_table_tx.h"

void blockchain_table_tx::addPut(PutOp data) {
  put_operations.emplace_back(std::move(data));
}

void blockchain_table_tx::addRemove(RemoveOp data) {
  remove_operations.emplace_back(std::move(data));
}

std::vector<PutOp>* blockchain_table_tx::getPutOperations() {
  return &put_operations;
}

std::vector<RemoveOp>* blockchain_table_tx::getRemoveOperations() {
  return &remove_operations;
}
