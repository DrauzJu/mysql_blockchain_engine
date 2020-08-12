#include "blockchain_table_tx.h"

blockchain_table_tx::blockchain_table_tx(THD* thd, int hton_slot, int prepare_immediately) {
  // Get or create transaction ID
  auto ha_data_ptr = thd->get_ha_data(hton_slot);
  auto ha_data = static_cast<ha_data_map *>(ha_data_ptr->ha_ptr);

  table_scan_data_filled = false;
  pending_remove_activated = false;
  commit_prepare_success = true;
  this->prepare_immediately = prepare_immediately;

  // Check if a table_tx object already exists: if yes, copy transaction ID
  for(auto& table_tx : *ha_data) {
    if(table_tx.second->tx != nullptr) {
      id = table_tx.second->tx->get_ID();
      return;
    }
  }

  boost::uuids::random_generator gen;
  id = gen();

  // log transaction id for debug purposes
  // std::cout << "Creating TX: " + get_printable_id() + "\n";
}

blockchain_table_tx::~blockchain_table_tx() {
  wait_for_commit_prepare_workers();
  commit_prepare_workers.clear();

  // log transaction id for debug purposes
  // std::cout << "Deleting TX: " + get_printable_id() + "\n";
}

void blockchain_table_tx::add_put(Put_op putOp, Connector* connector) {
  if(prepare_immediately) {
    // Send to blockchain tx buffer
    auto thread = std::thread([&, putOp, connector]() {
      Byte_data key(putOp.key.data->data(), putOp.key.data->size());
      Byte_data value(putOp.value.data->data(), putOp.value.data->size());
      int rc = connector->put(&key, &value, id);

      std::lock_guard<std::mutex> lock(commit_prepare_success_mtx);
      commit_prepare_success = std::min(commit_prepare_success, rc == 0);
    });
    commit_prepare_workers.emplace_back(std::move(thread));
  }

  apply_put_op_to_cache(putOp);
  put_operations.emplace_back(std::move(putOp));
}

void blockchain_table_tx::add_remove(Remove_op removeOp, bool pending, Connector* connector) {
  if(pending && pending_remove_activated) {
    pending_remove_operations.push(std::move(removeOp));
  } else {

    if(prepare_immediately) {
      // Send to blockchain tx buffer
      auto thread = std::thread([&, removeOp, connector]() {
        Byte_data bd(removeOp.key.data->data(), removeOp.key.data->size());
        int rc = connector->remove(&bd, id);

        std::lock_guard<std::mutex> lock(commit_prepare_success_mtx);
        commit_prepare_success = std::min(commit_prepare_success, rc == 0);
      });
      commit_prepare_workers.emplace_back(std::move(thread));
    }

    apply_remove_op_to_cache(removeOp);
    remove_operations.emplace_back(std::move(removeOp));
  }
}

std::vector<Put_op>* blockchain_table_tx::get_put_operations() {
  return &put_operations;
}

std::vector<Remove_op>* blockchain_table_tx::get_remove_operations() {
  return &remove_operations;
}

void blockchain_table_tx::apply_put_op_to_cache(Put_op & putOp) {
  table_scan_data[putOp.key] = putOp.value; // increments counter of internal shared_ptr
}

void blockchain_table_tx::apply_remove_op_to_cache(Remove_op & removeOp) {
  table_scan_data.erase(removeOp.key);
}

void blockchain_table_tx::reapply_pending_operations() {
  for(auto& putOp : put_operations) {
    apply_put_op_to_cache(putOp);
  }

  for(auto& removeOp : remove_operations) {
    apply_remove_op_to_cache(removeOp);
  }
}

void blockchain_table_tx::apply_pending_remove_ops(Connector* connector) {
  while (!pending_remove_operations.empty()) {
    auto pending_remove = std::move(pending_remove_operations.front());
    pending_remove_operations.pop();

    add_remove(pending_remove, false, connector);  // adds it to full list and applies to cache
  }
}

boost::uuids::uuid blockchain_table_tx::get_ID() {
  return id;
}

std::string blockchain_table_tx::get_printable_id() {
  std::stringstream ss;
  for(int i=0; i<16; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << (int) id.data[i];
  }
  
  return ss.str();
}

bool blockchain_table_tx::wait_for_commit_prepare_workers() {
  for(auto& thread : commit_prepare_workers) {
    if(thread.joinable()) thread.join();
  }

  return commit_prepare_success;
}

bool blockchain_table_tx::is_read_only() {
  if(prepare_immediately) {
    return commit_prepare_workers.empty(); // If no prepare workers, no put or remove operation exists
  } else {
    return get_put_operations()->empty() && get_remove_operations()->empty();
  }
}
