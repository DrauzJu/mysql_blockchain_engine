#include "blockchain_table_tx.h"

void blockchain_table_tx::addPut(PutOp data) {
  applyPutOpToCache(data);
  put_operations.emplace_back(std::move(data));
}

void blockchain_table_tx::addRemove(RemoveOp data, bool pending) {
  if(pending && pendingRemoveActivated) {
    pending_remove_operations.push(std::move(data));
  } else {
    applyRemoveOpToCache(data);
    remove_operations.emplace_back(std::move(data));
  }
}

std::vector<PutOp>* blockchain_table_tx::getPutOperations() {
  return &put_operations;
}

std::vector<RemoveOp>* blockchain_table_tx::getRemoveOperations() {
  return &remove_operations;
}

void blockchain_table_tx::applyPutOpToCache(PutOp & putOp) {
  tableScanData[putOp.key] = putOp.value; // increments counter of internal shared_ptr
}

void blockchain_table_tx::applyRemoveOpToCache(RemoveOp & removeOp) {
  tableScanData.erase(removeOp.key);
}

void blockchain_table_tx::reapplyPendingOperations() {
  for(auto& putOp : put_operations) {
    applyPutOpToCache(putOp);
  }

  for(auto& removeOp : remove_operations) {
    applyRemoveOpToCache(removeOp);
  }
}

void blockchain_table_tx::applyPendingRemoveOps() {
  while(!pending_remove_operations.empty()) {
    auto pendingRemove = std::move(pending_remove_operations.front());
    pending_remove_operations.pop();

    addRemove(pendingRemove, false); // adds it to full list and applies to cache
  }
}
