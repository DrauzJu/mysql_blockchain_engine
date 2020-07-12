#include "blockchain_table_tx.h"

blockchain_table_tx::blockchain_table_tx(THD* thd, int hton_slot) {
  // Get or create transaction ID
  auto ha_data_ptr = thd->get_ha_data(hton_slot);
  auto ha_data = static_cast<ha_data_map *>(ha_data_ptr->ha_ptr);

  // Check if a table_tx object already exists: if yes, copy transaction ID
  for(auto& table_tx : *ha_data) {
    if(table_tx.second->tx != nullptr) {
      id = table_tx.second->tx->getID();
      return;
    }
  }

  boost::uuids::random_generator gen;
  id = gen();
}

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
  while (!pending_remove_operations.empty()) {
    auto pendingRemove = std::move(pending_remove_operations.front());
    pending_remove_operations.pop();

    addRemove(pendingRemove,
              false);  // adds it to full list and applies to cache
  }
}

boost::uuids::uuid blockchain_table_tx::getID() {
  return id;
}
