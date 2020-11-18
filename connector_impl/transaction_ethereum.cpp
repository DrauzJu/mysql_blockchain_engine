/*
 * ---- TRANSACTION_ETHEREUM IMPLEMENTATION ----------------------------------
 */

#include "ethereum.h"
#include "transaction_ethereum.h"

void transaction_ethereum::translate_table_names(const std::vector<Table_name>& tables, std::vector<std::string>* addresses) {
  for(auto& table : tables) {
    addresses->emplace_back((*_table_contract_info)[table]);
  }
}


int transaction_ethereum::write_batch(std::vector<Put_op> * put_data, std::vector<Remove_op> * remove_data) {
  size_t total_size = put_data->size() + remove_data->size();

  std::stringstream data_string;
  data_string << numeric_to_hex(32);
  data_string << numeric_to_hex(total_size); // total number of operations

  // Put operations
  for(auto& put_op : *put_data) {
    // 1. Key
    auto bd_key = Byte_data(put_op.key.data->data(), put_op.key.data->size());
    data_string << byte_array_to_hex(&bd_key);

    // 2. Value
    auto bd_value = Byte_data(put_op.value.data->data(), put_op.value.data->size());
    data_string << byte_array_to_hex(&bd_value);

    // 3. Delete flag: false = 0
    data_string << numeric_to_hex(0);

    // 4. Address
    std::string address = (*_table_contract_info)[put_op.table];
    if(boost::starts_with(address, "0x")) {
      address = address.substr(2);  // remove "0x" at beginning of address
    }

    boost::to_lower(address);
    data_string << std::setw(64) << std::setfill('0') << address;
  } 
  
  // Delete operations
  for(auto& remove_op : *remove_data) {
    // 1. Key
    auto bd_key = Byte_data(remove_op.key.data->data(), remove_op.key.data->size());
    data_string << byte_array_to_hex(&bd_key);

    // 2. Value: all 0
    data_string << numeric_to_hex(0);

    // 3. Delete flag: true = 1
    data_string << numeric_to_hex(1);

    // 4. Address
    std::string address = (*_table_contract_info)[remove_op.table];
    if(boost::starts_with(address, "0x")) {
      address = address.substr(2);  // remove "0x" at beginning of address
    }

    boost::to_lower(address);
    data_string << std::setw(64) << std::setfill('0') << address;
  } 

  RPC_params params;
  params.method = "eth_sendTransaction";
  params.data = "0x1bf3eea5" + data_string.str();
  params.to = _commit_contract_address;

  // log("Data: " + params.data, "Write_Batch");

  // Create instance to have access to some helper methods
  Ethereum ethInstance(_connection_string, "", _from_address, _max_waiting_time);
  const std::string response = ethInstance.call(params, true);
  // log("Response: " + response, "Write_Batch");

  if (response.find("error") == std::string::npos) {
    log("success", "Write_Batch");
    return 0;
  } else {
    log("Failed: " + response, "Write_Batch");
    return 1;
  }
}

int transaction_ethereum::atomic_commit(TXID tx_ID, const std::vector<Table_name>& affected_tables) {
  // Create instance to have access to some helper methods
  Ethereum ethInstance(_connection_string, "", _from_address, _max_waiting_time);

  // Transate table names to addresses
  std::vector<std::string> addresses;
  translate_table_names(affected_tables, &addresses);

  Byte_data bdTxid(tx_ID.data, 16);
  std::string txidVal = byte_array_to_hex(&bdTxid, 32);

  std::stringstream address_string;
  address_string << std::setw(64) << std::setfill('0') << "40"; // some magic Ethereum number
  address_string << std::setw(64) << std::setfill('0') << addresses.size();
  for(auto address : addresses) {
    if(boost::starts_with(address, "0x")) {
      address = address.substr(2);  // remove "0x" at beginning of address
    }

    boost::to_lower(address);
    address_string << std::setw(64) << std::setfill('0') << address;
  }

  RPC_params params;
  params.method = "eth_sendTransaction";
  params.data = "0x334c1176" + txidVal + address_string.str();
  params.to = _commit_contract_address;

  const std::string response = ethInstance.call(params, true);

  if (response.find("error") == std::string::npos) {
    log("success", "atomicCommit");
    return 0;
  } else {
    log("Failed: " + response, "atomicCommit");
    return 1;
  }
}

/*
{
	"334c1176": "commitAll(bytes16,address[])",
	"1bf3eea5": "writeBatch((bytes32,bytes32,bool,address)[])"
}
*/
