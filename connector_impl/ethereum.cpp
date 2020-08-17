#include "ethereum.h"

/*
 * ---- HELPER METHODS ----------------------------------
 */

void log(const std::string& msg, const std::string& method = "") {
    const std::string m = method.empty() ? "] " : "- " + method + "] ";
    std::cout << "[ETHEREUM " << m << msg << std::endl;
}

static size_t write_callback(char *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append(contents, size * nmemb);
    return size * nmemb;
}

static void parse_32byte_hex_string(const std::string& s, uint8_t* out, size_t length) {
    const char* hex_string = s.c_str();

    for(size_t i=0; i<length; i++) {
        char byte_string[3];
        byte_string[0] = hex_string[2*i];
        byte_string[1] = hex_string[2*i+1];
        byte_string[2] = 0; // termination character

        // convert 1-byte hex string to numeric representation
        auto sNum = (uint8_t) strtoul(byte_string, nullptr, 16);
        out[i] = sNum;
    }
}

static std::string byte_array_to_hex(Byte_data* data, int length=32) {
    assert(data->data_size <= 32);

    std::stringstream ss;
    ss << std::hex;
    int i=0;
    for (; i<data->data_size; i++)
      ss << std::setw(2) << std::setfill('0') << (int) (data->data[i]);

    for(; i<length; i++)
      ss << std::setw(2) << std::setfill('0') << 0;

    return ss.str();
}

template<typename T>
static std::string numeric_to_hex(T num, int size=64) {
  std::stringstream ss;
  ss << std::hex;
  ss << std::setw(size) << std::setfill('0') << num;

  return ss.str();
}

static std::vector<std::string> split(const std::string& str, int split_length) {
    ulong num_substrings = str.length() / split_length;
    std::vector<std::string> ret;
    ret.reserve(num_substrings);

    for (ulong i = 0; i < num_substrings; i++) {
        ret.push_back(str.substr(i * split_length, split_length));
    }

    // If there are leftover characters, create a shorter item at the end.
    if (str.length() % split_length != 0) {
        ret.push_back(str.substr(split_length * num_substrings));
    }

    return ret;
}

static std::string parse_params_to_json(const RPC_params& params) {
    std::vector<std::string> els;
    std::string json = "{";

    if (!params.from.empty()) els.push_back(R"("from":")" + params.from + "\"");
    if (!params.data.empty()) els.push_back(R"("data":")" + params.data + "\"");
    if (!params.to.empty()) els.push_back(R"("to":")" + params.to + "\"");
    if (!params.gas.empty()) els.push_back(R"("gas":")" + params.gas + "\"");
    if (!params.gas_price.empty()) els.push_back(R"("gasPrice":")" + params.gas_price + "\"");
    if (params.nonce > 0) {
      std::stringstream ss;
      ss << "0x";
      ss << numeric_to_hex(params.nonce, 0); // no leading zeros
      els.push_back(R"("nonce":")" + ss.str() + "\"");
    }

    for (std::vector<int>::size_type i = 0; i < els.size(); i++) {
        json += els[i];
        if (i < els.size() - 1) json += ",";
    }

    return json + "}";
}

/*
 * ---- ETHEREUM IMPLEMENTATION ----------------------------------
 */

Ethereum::Ethereum(std::string connection_string,
                   std::string store_contract_address,
                   std::string from_address,
                   int max_waiting_time) {
    _store_contract_address = std::move(store_contract_address);
    _from_address = std::move(from_address);
    _connection_string = std::move(connection_string);
    this->max_waiting_time = max_waiting_time * 1000; // convert to ms

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, _connection_string.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    log("Contract Address: " + _store_contract_address);

    // Init nonce
    std::lock_guard<std::mutex> lock(nonce_init_mtx);
    if(Ethereum::nonce == 0) {
      RPC_params params;
      params.method = "eth_getTransactionCount";
      params.quantity_tag = "latest";
      std::string param = "\"" + _from_address + R"(", "latest")";
      std::string method = "eth_getTransactionCount";

      auto response = call(param, method);
      try {
        auto json = nlohmann::json::parse(response);
        auto hex_count = json["result"].get<std::string>().substr(2); // remove 0x
        auto tmp_nonce = strtoul(hex_count.c_str(), nullptr, 16);
        tmp_nonce = std::max((ulong) 0, tmp_nonce - 1); // nonce starts at 0
        Ethereum::nonce.store(tmp_nonce);
      } catch (std::exception&) {
        std::cerr << "[BLOCKCHAIN] - Can not parse eth_getTransactionCount response!" << std::endl;
      }
    }

    log("Ethereum nonce is " + std::to_string(Ethereum::nonce.load()));
}

Ethereum::~Ethereum() {
    curl_easy_cleanup(curl);
}

int Ethereum::get(Byte_data* key, unsigned char* buf, int value_size) {
  std::string hexKey = byte_array_to_hex(key);

  RPC_params params;
  params.method = "eth_call";
  params.data = "0x8eaa6ac0" + hexKey;
  params.quantity_tag = "latest";
  // log("Data: " + params.data, "Get");

  const std::string response = call(params, false);
  // log("Response: " + response, "Get");


  if (response.find("error") == std::string::npos) {
    log("success", "Get");

    // extract result
    std::regex rgx(".*\"result\":\"0x(\\w+)\".*");
    std::smatch match;

    if (std::regex_search(response.begin(), response.end(), match, rgx)) {
      std::string result = match[1];

      // Copy key
      memcpy(&(buf[0]), key->data, key->data_size);

      // Extract value and save in buf
      std::vector<uint8_t> value(value_size);
      parse_32byte_hex_string(result, &(buf[key->data_size]), value_size);

      return 0;
    } else {
      log("No value for key found", "Get");
      return HA_ERR_END_OF_FILE;
    }
  } else {
    log("Failed: no result found", "Get");
    return 1;
  }
}

int Ethereum::put(Byte_data* key, Byte_data* value, TXID txid) {

    std::string hex_key = byte_array_to_hex(key);
    std::string hex_val = byte_array_to_hex(value);

    Byte_data bdTxid(txid.data, 16);
    std::string txid_val = byte_array_to_hex(&bdTxid);

    RPC_params params;
    params.method = "eth_sendTransaction";

    if(txid.is_nil()) {
      params.data = "0x4c667080" + hex_key + hex_val;
    } else {
      params.data = "0x3c58dd03" + hex_key + hex_val + txid_val;
    }

    // log("Data: " + params.data, "Put");

    const std::string response = call(params, true);
    // log("Response: " + response, "Put");


    if (response.find("error") == std::string::npos) {
        log("success", "Put");
        return 0;
    } else {
        log("Failed: " + response, "Put");
        return 1;
    }
}

int Ethereum::put_batch(std::vector<Put_op>* data, TXID txid) {
  auto size = data->size();

  std::stringstream data_string;
  if(txid.is_nil()) {
    data_string << numeric_to_hex(64);
    data_string << numeric_to_hex(96 + 32 * size);
  } else {
    data_string << numeric_to_hex(96);
    data_string << numeric_to_hex(128 + 32 * size);

    Byte_data bdTxid(txid.data, 16);
    data_string << byte_array_to_hex(&bdTxid);
  }

  // All keys
  data_string << numeric_to_hex(size); // number of keys
  for(ulong i=0; i<size; i++) {
    auto& put_op = data->at(i);
    auto bd = Byte_data(put_op.key.data->data(), put_op.key.data->size());
    data_string << byte_array_to_hex(&bd);
  }

  // All values
  data_string << numeric_to_hex(size); // number of values
  for(ulong i=0; i<size; i++) {
    auto& put_op = data->at(i);
    auto bd = Byte_data(put_op.value.data->data(), put_op.value.data->size());
    data_string << byte_array_to_hex(&bd);
  }

  RPC_params params;
  params.method = "eth_sendTransaction";

  if(txid.is_nil()) {
    params.data = "0x9b36675c" + data_string.str();
  } else {
    params.data = "0x0238a793" + data_string.str();
  }

  // log("Data: " + params.data, "PutBatch");

  const std::string response = call(params, true);
  // log("Response: " + response, "PutBatch");


  if (response.find("error") == std::string::npos) {
    log("success", "Put_batch");
    return 0;
  } else {
    log("Failed: " + response, "Put_batch");
    return 1;
  }

}

int Ethereum::remove(Byte_data *key, TXID txid) {

    std::string hex_key = byte_array_to_hex(key);
    Byte_data bdTxid(txid.data, 16);
    std::string txid_val = byte_array_to_hex(&bdTxid);

    RPC_params params;
    params.method = "eth_sendTransaction";

    if(txid.is_nil()) {
      params.data = "0x95bc2673" + hex_key;
    } else {
      params.data = "0x29a32c0a" + hex_key + txid_val;
    }
    // log("Data: " + params.data, "Remove");

    const std::string response = call(params, true);
    // log("Response: " + response, "Remove");


    if (response.find("error") == std::string::npos) {
        log("success", "Remove");
        return 0;
    } else {
        log("Failed: " + response, "Remove");
        return 1;
    }
}

int Ethereum::remove_batch(std::vector<Remove_op> * data, TXID txid) {
  auto size = data->size();

  std::stringstream data_string;
  if(txid.is_nil()) {
    data_string << numeric_to_hex(32);
  } else {
    data_string << numeric_to_hex(64);

    Byte_data bdTxid(txid.data, 16);
    data_string << byte_array_to_hex(&bdTxid);
  }

  // All keys
  data_string << numeric_to_hex(size); // number of keys
  for(ulong i=0; i<size; i++) {
    auto& remove_op = data->at(i);
    auto bd = Byte_data(remove_op.key.data->data(), remove_op.key.data->size());
    data_string << byte_array_to_hex(&bd);
  }

  RPC_params params;
  params.method = "eth_sendTransaction";

  if(txid.is_nil()) {
    params.data = "0x2d9bb756" + data_string.str();
  } else {
    params.data = "0x702de045" + data_string.str();
  }

  // log("Data: " + params.data, "removeBatch");

  const std::string response = call(params, true);
  // log("Response: " + response, "removeBatch");

  if (response.find("error") == std::string::npos) {
    log("success", "remove_batch");
    return 0;
  } else {
    log("Failed: " + response, "remove_batch");
    return 1;
  }
}

void Ethereum::table_scan_to_vec(std::vector<Managed_byte_data> &tuples,
                              const size_t key_length, const size_t value_length) {

  auto results = table_scan_call();
  unsigned int count = get_table_scan_results_size(results);
  if(count == 0) {
    return;
  }

  log("success", "table_scan_to_vec");

  for (std::vector<int>::size_type i = 3; i < 3 + count; i++) {
    std::vector<int>::size_type value_index = i + count + 1;

    auto tuple = Managed_byte_data(key_length + value_length);

    parse_32byte_hex_string(results[i], tuple.data->data(), key_length);
    parse_32byte_hex_string(results[value_index], &((*tuple.data)[key_length]), value_length);

    tuples.emplace_back(std::move(tuple));
  }

}

void Ethereum::table_scan_to_map(tx_cache_t& tuples,
                              size_t key_length, size_t value_length) {

  auto results = table_scan_call();
  unsigned int count = get_table_scan_results_size(results);
  if(count == 0) {
    return;
  }

  log("success", "table_scan_to_map");

  for (std::vector<int>::size_type i = 3; i < 3 + count; i++) {
    std::vector<int>::size_type valueIndex = i + count + 1;

    auto key = Managed_byte_data(key_length);
    auto value = Managed_byte_data(value_length);

    parse_32byte_hex_string(results[i], key.data->data(), key_length);
    parse_32byte_hex_string(results[valueIndex], value.data->data(), value_length);

    tuples[key] = std::move(value);
  }
}

std::vector<std::string> Ethereum::table_scan_call() {
  RPC_params params;
  params.method = "eth_call";
  params.data = "0xb3055e26";
  params.quantity_tag = "latest";

  const std::string s = call(params, false);

  std::string rpc_result;
  try {
    auto json = nlohmann::json::parse(s);
    rpc_result = json["result"];
    rpc_result = rpc_result.substr(2);
  } catch (std::exception&) {
    std::cerr << "[BLOCKCHAIN] - Can not parse TableScan response!" << std::endl;
  }

  return split(rpc_result, 64);
}

size_t Ethereum::get_table_scan_results_size(std::vector<std::string> response) {
  // Extract number of tuples
  unsigned int count;
  if(response.size() > 2) {
    std::stringstream ss;
    ss << std::hex << response[2];
    ss >> count;
    return count;
  } else {
    return 0;
  }
}

int Ethereum::drop_table() {
  // todo: implement: call function drop() of contract
  return 0;
}

std::string Ethereum::check_mining_result(std::string& transaction_ID) {
  std::string response;
  size_t waited = 0;

  while((waited + MINING_CHECK_INTERVAL) < this->max_waiting_time) {
    std::this_thread::sleep_for (std::chrono::milliseconds (MINING_CHECK_INTERVAL));
    std::string transactionParam = "\"" + transaction_ID + "\"";
    std::string method = "eth_getTransactionByHash";
    response = call(transactionParam, method);

    try {
      nlohmann::json jsonResponse = nlohmann::json::parse(response);

      if(!(jsonResponse.at("result").at("blockNumber").is_null())) {
        std::stringstream msg;
        msg << "Mining took about " << waited << " ms";
        log(msg.str(), "checkMiningResult");
        return response;
      }
    } catch (nlohmann::detail::exception& ) {
      log("Can't parse " + response, "checkMiningResult");
      // continue, so try again
    }

    waited += MINING_CHECK_INTERVAL;
  }


  std::stringstream msg;
  msg << "Failed to get transaction block number after " << this->max_waiting_time << " ms";
  log(msg.str());

  throw Transaction_confirmation_exception("Transaction was not mined!", transaction_ID);
}


std::string Ethereum::call(RPC_params params, bool set_gas) {
  params.from = _from_address;
  if(params.to.empty()) params.to = _store_contract_address;
  if(set_gas) params.gas = "0x7A120";

  // Increment nonce to indicate that Ethereum should not replace a currently
  // pending transaction, but add as new transaction
  if(params.method == "eth_sendTransaction") params.nonce = ++nonce;

  std::string json = parse_params_to_json(params);
  const std::string quantity_tag = params.quantity_tag.empty() ? "" : ",\"" + params.quantity_tag + "\"";
  json = json + quantity_tag;

  std::string response;
  try {
    return call(json, params.method);
  } catch (Transaction_nonce_exception& ex) {
    // Retry, which will increase nonce
    log("Retrying ETH transaction with higher nonce", "Call");
    return call(params, set_gas);
  } catch (Transaction_confirmation_exception& ex) {
    return "error: " + std::string(ex.what());
  }
}

std::string Ethereum::call(std::string& params, std::string& method) {
  std::string read_buffer_call;
  const std::string post_data = R"({"jsonrpc":"2.0","id":1,"method":")" + method + R"(","params":[)" + params + "]}";
  // log("Body: " + postData, "Call");

  if (curl) {
    std::lock_guard<std::mutex> lock(curl_call_mtx);
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer_call);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    CURLcode res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
      auto msg = "CURL perform() returned an error: " + std::string(curl_easy_strerror(res));
      log(msg, "Call");
    }
    
  } else log("no curl", "Call");

  std::string read_buffer;
  if(method == "eth_sendTransaction") {
    try {
      nlohmann::json json_response = nlohmann::json::parse(read_buffer_call);

      if(json_response.contains("error")) {
        auto errorMsg = json_response.at("error").at("message").get<std::string>();
        if(errorMsg == "already known" || errorMsg == "nonce too low") {
          throw Transaction_nonce_exception();
        } else {
          std::string msg = "Unknown transaction error: " + errorMsg;
          log(msg, "Call");
          read_buffer = "error: " + errorMsg;
        }
      } else {
        auto result = json_response["result"].get<std::string>();
        read_buffer = check_mining_result(result);
      }
    } catch (nlohmann::detail::exception& ) {
      read_buffer = "error: Can not parse response from eth_sendTransaction, so unable to check mining result";
      log("Error parsing call response: " + read_buffer_call, "Call");
    }
  } else {
    read_buffer = read_buffer_call;
  }

  return read_buffer;
}

int Ethereum::clear_commit_prepare(boost::uuids::uuid tx_ID) {
  Byte_data bdTxid(tx_ID.data, 16);
  std::string txidVal = byte_array_to_hex(&bdTxid);

  RPC_params params;
  params.method = "eth_sendTransaction";
  params.data = "0x93ec62c1" + txidVal;
  // log("Data: " + params.data, "clearCommitPrepare");

  const std::string response = call(params, true);
  // log("Response: " + response, "clearCommitPrepare");

  if (response.find("error") == std::string::npos) {
    log("success", "ClearTX");
    return 0;
  } else {
    log("Failed: " + response, "ClearTX");
    return 1;
  }
}

int Ethereum::atomic_commit(std::string connection_string,
                           std::string from_address,
                           int max_waiting_time,
                           std::string commit_contract_address, TXID tx_ID,
                           const std::vector<std::string>& addresses) {
  Ethereum ethInstance(std::move(connection_string), "",
                       std::move(from_address), max_waiting_time);

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
  params.to = std::move(commit_contract_address);

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
 * {
	"93ec62c1": "clean(bytes16)",
	"8fcdc9a9": "commit(bytes16)",
	"f751cd8f": "drop()",
	"8eaa6ac0": "get(bytes32)",
	"50a5fd68": "getBatch(bytes32[])",
	"4c667080": "put(bytes32,bytes32)",
	"3c58dd03": "put(bytes32,bytes32,bytes16)",
	"9b36675c": "putBatch(bytes32[],bytes32[])",
	"0238a793": "putBatch(bytes32[],bytes32[],bytes16)",
	"95bc2673": "remove(bytes32)",
	"29a32c0a": "remove(bytes32,bytes16)",
        "2d9bb756": "removeBatch(bytes32[])",
        "702de045": "removeBatch(bytes32[],bytes16)",
	"b3055e26": "tableScan()"
}

{
        "334c1176": "commitAll(bytes16,address[])"
}
 */
