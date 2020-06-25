#include "blockchain_tx.h"

void blockchain_tx::addPut(std::unique_ptr<PutOp> data) {
  put_operations.emplace_back(std::move(data));
}

void blockchain_tx::addRemove(std::unique_ptr<RemoveOp> data) {
  remove_operations.emplace_back(std::move(data));
}

std::vector<std::unique_ptr<PutOp>>* blockchain_tx::getPutOpearations() {
  return &put_operations;
}

std::vector<std::unique_ptr<RemoveOp>>* blockchain_tx::getRemoveOpearations() {
  return &remove_operations;
}
