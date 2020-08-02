#include "blockchain_table_tx.h"

blockchain_table_tx::blockchain_table_tx(THD* thd, int hton_slot, int prepare_immediately) {
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

  // log transaction id for debug purposes
  /*std::cout << "New transaction: ";
  for(int i=0; i<16; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int) id.data[i];
  }
  std::cout << std::endl;*/

  tableScanDataFilled = false;
  pendingRemoveActivated = false;
  commitPrepareSuccess = true;
  prepareImmediately = prepare_immediately;
}

void blockchain_table_tx::addPut(PutOp putOp, Connector* connector) {
  if(prepareImmediately) {
    // Send to blockchain tx buffer
    auto thread = std::thread([&, putOp, connector]() {
      ByteData key(putOp.key.data->data(), putOp.key.data->size());
      ByteData value(putOp.value.data->data(), putOp.value.data->size());
      int rc = connector->put(&key, &value, id);

      std::lock_guard lock(commitPrepareSuccessMtx);
      commitPrepareSuccess = std::min(commitPrepareSuccess, rc == 0);
    });
    commitPrepareWorkers.emplace_back(std::move(thread));
  }

  applyPutOpToCache(putOp);
  put_operations.emplace_back(std::move(putOp));
}

void blockchain_table_tx::addRemove(RemoveOp removeOp, bool pending, Connector* connector) {
  if(pending && pendingRemoveActivated) {
    pending_remove_operations.push(std::move(removeOp));
  } else {

    if(prepareImmediately) {
      // Send to blockchain tx buffer
      auto thread = std::thread([&, removeOp, connector]() {
        ByteData bd(removeOp.key.data->data(), removeOp.key.data->size());
        int rc = connector->remove(&bd, id);

        std::lock_guard lock(commitPrepareSuccessMtx);
        commitPrepareSuccess = std::min(commitPrepareSuccess, rc == 0);
      });
      commitPrepareWorkers.emplace_back(std::move(thread));
    }

    applyRemoveOpToCache(removeOp);
    remove_operations.emplace_back(std::move(removeOp));
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

void blockchain_table_tx::applyPendingRemoveOps(Connector* connector) {
  while (!pending_remove_operations.empty()) {
    auto pendingRemove = std::move(pending_remove_operations.front());
    pending_remove_operations.pop();

    addRemove(pendingRemove, false, connector);  // adds it to full list and applies to cache
  }
}

boost::uuids::uuid blockchain_table_tx::getID() {
  return id;
}

bool blockchain_table_tx::waitForCommitPrepareWorkers() {
  for(auto& thread : commitPrepareWorkers) {
    if(thread.joinable()) thread.join();
  }

  return commitPrepareSuccess;
}
