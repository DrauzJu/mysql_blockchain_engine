#include "ethereum.h"

/*
 * ---- HELPER METHODS ----------------------------------
 */

void log(const std::string& msg, const std::string& method = "") {
    const std::string m = method.empty() ? "] " : "- " + method + "] ";
    std::cout << "[ETHEREUM " << m << msg << std::endl;
}

static size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append(contents, size * nmemb);
    return size * nmemb;
}

static void parse32ByteHexString(const std::string& s, uint8_t* out, size_t length) {
    const char* hexString = s.c_str();

    for(size_t i=0; i<length; i++) {
        char byteString[3];
        byteString[0] = hexString[2*i];
        byteString[1] = hexString[2*i+1];
        byteString[2] = 0; // termination character

        // convert 1-byte hex string to numeric representation
        auto sNum = (uint8_t) strtoul(byteString, nullptr, 16);
        out[i] = sNum;
    }
}

static std::string byteArrayToHex(ByteData* data, int length=32) {
    assert(data->dataSize <= 32);

    std::stringstream ss;
    ss << std::hex;
    int i=0;
    for (; i<data->dataSize; i++)
      ss << std::setw(2) << std::setfill('0') << (int) (data->data[i]);

    for(; i<length; i++)
      ss << std::setw(2) << std::setfill('0') << 0;

    return ss.str();
}

template<typename T>
static std::string numericToHex(T num, int size=64) {
  std::stringstream ss;
  ss << std::hex;
  ss << std::setw(size) << std::setfill('0') << num;

  return ss.str();
}

static std::vector<std::string> Split(const std::string& str, int splitLength) {
    ulong NumSubstrings = str.length() / splitLength;
    std::vector<std::string> ret;
    ret.reserve(NumSubstrings);

    for (ulong i = 0; i < NumSubstrings; i++) {
        ret.push_back(str.substr(i * splitLength, splitLength));
    }

    // If there are leftover characters, create a shorter item at the end.
    if (str.length() % splitLength != 0) {
        ret.push_back(str.substr(splitLength * NumSubstrings));
    }

    return ret;
}

static std::string parseParamsToJson(const RPCparams& params) {
    std::vector<std::string> els;
    std::string json = "{";

    if (!params.from.empty()) els.push_back(R"("from":")" + params.from + "\"");
    if (!params.data.empty()) els.push_back(R"("data":")" + params.data + "\"");
    if (!params.to.empty()) els.push_back(R"("to":")" + params.to + "\"");
    if (!params.gas.empty()) els.push_back(R"("gas":")" + params.gas + "\"");
    if (!params.gasPrice.empty()) els.push_back(R"("gasPrice":")" + params.gasPrice + "\"");
    if (params.nonce > 0) {
      std::stringstream ss;
      ss << "0x";
      ss << numericToHex(params.nonce, 0); // no leading zeros
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

Ethereum::Ethereum(std::string connectionString,
                   std::string storeContractAddress,
                   std::string fromAddress,
                   int maxWaitingTime) {
    _storeContractAddress = std::move(storeContractAddress);
    _fromAddress = std::move(fromAddress);
    _connectionString = std::move(connectionString);
    this->maxWaitingTime = maxWaitingTime * 1000; // convert to ms

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, _connectionString.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    log("Contract Address: " + _storeContractAddress);

    // Init nonce
    std::lock_guard<std::mutex> lock(nonceInitMtx);
    if(Ethereum::nonce == 0) {
      RPCparams params;
      params.method = "eth_getTransactionCount";
      params.quantity_tag = "latest";
      std::string param = "\"" + _fromAddress + R"(", "latest")";
      std::string method = "eth_getTransactionCount";

      auto response = call(param, method);
      try {
        auto json = nlohmann::json::parse(response);
        auto hexCount = json["result"].get<std::string>().substr(2); // remove 0x
        auto tmpNonce = strtoul(hexCount.c_str(), nullptr, 16);
        tmpNonce = std::max((ulong) 0, tmpNonce - 1); // nonce starts at 0
        Ethereum::nonce.store(tmpNonce);
      } catch (std::exception&) {
        std::cerr << "[BLOCKCHAIN] - Can not parse eth_getTransactionCount response!" << std::endl;
      }
    }

    log("Ethereum nonce is " + std::to_string(Ethereum::nonce.load()));
}

Ethereum::~Ethereum() {
    curl_easy_cleanup(curl);
}

int Ethereum::get(ByteData* key, unsigned char* buf, int value_size) {
  std::string hexKey = byteArrayToHex(key);

  RPCparams params;
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
      memcpy(&(buf[0]), key->data, key->dataSize);

      // Extract value and save in buf
      std::vector<uint8_t> value(value_size);
      parse32ByteHexString(result, &(buf[key->dataSize]), value_size);

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

int Ethereum::put(ByteData* key, ByteData* value, TXID txid) {

    std::string hexKey = byteArrayToHex(key);
    std::string hexVal = byteArrayToHex(value);

    ByteData bdTxid(txid.data, 16);
    std::string txidVal = byteArrayToHex(&bdTxid);

    RPCparams params;
    params.method = "eth_sendTransaction";

    if(txid.is_nil()) {
      params.data = "0x4c667080" + hexKey + hexVal;
    } else {
      params.data = "0x3c58dd03" + hexKey + hexVal + txidVal;
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

int Ethereum::putBatch(std::vector<PutOp>* data, TXID txid) {
  auto size = data->size();

  std::stringstream dataString;
  if(txid.is_nil()) {
    dataString << numericToHex(64);
    dataString << numericToHex(96 + 32 * size);
  } else {
    dataString << numericToHex(96);
    dataString << numericToHex(128 + 32 * size);

    ByteData bdTxid(txid.data, 16);
    dataString << byteArrayToHex(&bdTxid);
  }

  // All keys
  dataString << numericToHex(size); // number of keys
  for(ulong i=0; i<size; i++) {
    auto& putOp = data->at(i);
    auto bd = ByteData(putOp.key.data->data(), putOp.key.data->size());
    dataString << byteArrayToHex(&bd);
  }

  // All values
  dataString << numericToHex(size); // number of values
  for(ulong i=0; i<size; i++) {
    auto& putOp = data->at(i);
    auto bd = ByteData(putOp.value.data->data(), putOp.value.data->size());
    dataString << byteArrayToHex(&bd);
  }

  RPCparams params;
  params.method = "eth_sendTransaction";

  if(txid.is_nil()) {
    params.data = "0x9b36675c" + dataString.str();
  } else {
    params.data = "0x0238a793" + dataString.str();
  }

  // log("Data: " + params.data, "PutBatch");

  const std::string response = call(params, true);
  // log("Response: " + response, "PutBatch");


  if (response.find("error") == std::string::npos) {
    log("success", "PutBatch");
    return 0;
  } else {
    log("Failed: " + response, "PutBatch");
    return 1;
  }

}

int Ethereum::remove(ByteData *key, TXID txid) {

    std::string hexKey = byteArrayToHex(key);
    ByteData bdTxid(txid.data, 16);
    std::string txidVal = byteArrayToHex(&bdTxid);

    RPCparams params;
    params.method = "eth_sendTransaction";

    if(txid.is_nil()) {
      params.data = "0x95bc2673" + hexKey;
    } else {
      params.data = "0x29a32c0a" + hexKey + txidVal;
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

int Ethereum::removeBatch(std::vector<RemoveOp> * data, TXID txid) {
  auto size = data->size();

  std::stringstream dataString;
  if(txid.is_nil()) {
    dataString << numericToHex(32);
  } else {
    dataString << numericToHex(64);

    ByteData bdTxid(txid.data, 16);
    dataString << byteArrayToHex(&bdTxid);
  }

  // All keys
  dataString << numericToHex(size); // number of keys
  for(ulong i=0; i<size; i++) {
    auto& removeOp = data->at(i);
    auto bd = ByteData(removeOp.key.data->data(), removeOp.key.data->size());
    dataString << byteArrayToHex(&bd);
  }

  RPCparams params;
  params.method = "eth_sendTransaction";

  if(txid.is_nil()) {
    params.data = "0x2d9bb756" + dataString.str();
  } else {
    params.data = "0x702de045" + dataString.str();
  }

  // log("Data: " + params.data, "removeBatch");

  const std::string response = call(params, true);
  // log("Response: " + response, "removeBatch");

  if (response.find("error") == std::string::npos) {
    log("success", "removeBatch");
    return 0;
  } else {
    log("Failed: " + response, "removeBatch");
    return 1;
  }
}

void Ethereum::tableScanToVec(std::vector<ManagedByteData> &tuples,
                              const size_t keyLength, const size_t valueLength) {

  auto results = tableScanCall();
  unsigned int count = getTableScanResultsSize(results);
  if(count == 0) {
    return;
  }

  for (std::vector<int>::size_type i = 3; i < 3 + count; i++) {
    std::vector<int>::size_type valueIndex = i + count + 1;

    auto tuple = ManagedByteData(keyLength + valueLength);

    parse32ByteHexString(results[i], tuple.data->data(), keyLength);
    parse32ByteHexString(results[valueIndex], &((*tuple.data)[keyLength]), valueLength);

    tuples.emplace_back(std::move(tuple));
  }

}

void Ethereum::tableScanToMap(tx_cache_t& tuples,
                              size_t keyLength, size_t valueLength) {

  auto results = tableScanCall();
  unsigned int count = getTableScanResultsSize(results);
  if(count == 0) {
    return;
  }

  for (std::vector<int>::size_type i = 3; i < 3 + count; i++) {
    std::vector<int>::size_type valueIndex = i + count + 1;

    auto key = ManagedByteData(keyLength);
    auto value = ManagedByteData(valueLength);

    parse32ByteHexString(results[i], key.data->data(), keyLength);
    parse32ByteHexString(results[valueIndex], value.data->data(), valueLength);

    tuples[key] = std::move(value);
  }
}

std::vector<std::string> Ethereum::tableScanCall() {
  RPCparams params;
  params.method = "eth_call";
  params.data = "0xb3055e26";
  params.quantity_tag = "latest";

  const std::string s = call(params, false);

  std::string rpcResult;
  try {
    auto json = nlohmann::json::parse(s);
    rpcResult = json["result"];
    rpcResult = rpcResult.substr(2);
  } catch (std::exception&) {
    std::cerr << "[BLOCKCHAIN] - Can not parse TableScan response!" << std::endl;
  }

  return Split(rpcResult, 64);
}

size_t Ethereum::getTableScanResultsSize(std::vector<std::string> response) {
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

int Ethereum::dropTable() {
  // todo: implement: call function drop() of contract
  return 0;
}

std::string Ethereum::checkMiningResult(std::string& transactionID) {
  std::string response;
  size_t waited = 0;

  while((waited + MINING_CHECK_INTERVAL) < this->maxWaitingTime) {
    std::this_thread::sleep_for (std::chrono::milliseconds (MINING_CHECK_INTERVAL));
    std::string transactionParam = "\"" + transactionID + "\"";
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
  msg << "Failed to get transaction block number after " << this->maxWaitingTime << " ms";
  log(msg.str());

  throw TransactionConfirmationException("Transaction was not mined!", transactionID);
}


std::string Ethereum::call(RPCparams params, bool setGas, bool ) {
  params.from = _fromAddress;
  if(params.to.empty()) params.to = _storeContractAddress;
  if(setGas) params.gas = "0x7A120";

  // Increment nonce to indicate that Ethereum should not replace a currently
  // pending transaction, but add as new transaction
  if(params.method == "eth_sendTransaction") params.nonce = ++nonce;

  std::string json = parseParamsToJson(params);
  const std::string quantity_tag = params.quantity_tag.empty() ? "" : ",\"" + params.quantity_tag + "\"";
  json = json + quantity_tag;

  std::string response;
  try {
    return call(json, params.method);
  } catch (TransactionNonceException& ex) {
    // Retry, which will increase nonce
    log("Retrying ETH transaction with higher nonce", "Call");
    return call(params, setGas, false);
  } catch (TransactionConfirmationException& ex) {
    return "error: " + std::string(ex.what());

    // Idea: increase gas price and try again
    // Currently does not work, since gas price of transaction is always 0
    /*if(isRetry) {
      // it also failed on retry --> exit
      return "error: " + std::string(ex.what());
    }

    // Retry with higher gas price
    std::string transactionParam = "\"" + ex.transaction + "\"";
    std::string method = "eth_getTransactionByHash";
    response = call(transactionParam, method);
    ulong gasPrice;

    try {
      nlohmann::json jsonResponse = nlohmann::json::parse(response);
      auto gasPriceHex = jsonResponse.at("result").at("gasPrice").get<std::string>();
      gasPrice = strtoul(gasPriceHex.substr(2).c_str(), nullptr, 16);
    } catch (nlohmann::detail::exception& ) {
      log("Can't parse " + response, "Call");
    }

    log("Retrying ETH transaction with higher gas price", "Call");
    gasPrice *= 1.2; // Increase gas price by 20%
    params.gasPrice = "0x" + numericToHex(gasPrice, 0);
    return call(params, setGas, true);*/
  }
}

std::string Ethereum::call(std::string& params, std::string& method) {
  std::string readBufferCall;
  const std::string postData = R"({"jsonrpc":"2.0","id":1,"method":")" + method + R"(","params":[)" + params + "]}";
  // log("Body: " + postData, "Call");

  if (curl) {
    std::lock_guard<std::mutex> lock(curlCallMtx);
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBufferCall);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    CURLcode res = curl_easy_perform(curl);

    if(res != CURLE_OK) {
      auto msg = "CURL perform() returned an error: " + std::string(curl_easy_strerror(res));
      log(msg, "Call");
    }
    
  } else log("no curl", "Call");

  std::string readBuffer;
  if(method == "eth_sendTransaction") {
    try {
      nlohmann::json jsonResponse = nlohmann::json::parse(readBufferCall);

      if(jsonResponse.contains("error")) {
        auto errorMsg = jsonResponse.at("error").at("message").get<std::string>();
        if(errorMsg == "already known" || errorMsg == "nonce too low") {
          throw TransactionNonceException();
        } else {
          std::string msg = "Unknown transaction error: " + errorMsg;
          log(msg, "Call");
          readBuffer = "error: " + errorMsg;
        }
      } else {
        auto result = jsonResponse["result"].get<std::string>();
        readBuffer = checkMiningResult(result);
      }
    } catch (nlohmann::detail::exception& ) {
      readBuffer = "error: Can not parse response from eth_sendTransaction, so unable to check mining result";
      log("Error parsing call response: " + readBufferCall, "Call");
    }
  } else {
    readBuffer = readBufferCall;
  }

  return readBuffer;
}

int Ethereum::clearCommitPrepare(boost::uuids::uuid txid) {
  ByteData bdTxid(txid.data, 16);
  std::string txidVal = byteArrayToHex(&bdTxid);

  RPCparams params;
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

int Ethereum::atomicCommit(std::string connectionString,
                           std::string fromAddress,
                           int maxWaitingTime,
                           std::string commitContractAddress, TXID txID,
                           const std::vector<std::string>& addresses) {
  Ethereum ethInstance(std::move(connectionString), "",
                       std::move(fromAddress), maxWaitingTime);

  ByteData bdTxid(txID.data, 16);
  std::string txidVal = byteArrayToHex(&bdTxid, 32);

  std::stringstream addressString;
  addressString << std::setw(64) << std::setfill('0') << "40"; // some magic Ethereum number
  addressString << std::setw(64) << std::setfill('0') << addresses.size();
  for(auto address : addresses) {
    if(boost::starts_with(address, "0x")) {
      address = address.substr(2);  // remove "0x" at beginning of address
    }

    boost::to_lower(address);
    addressString << std::setw(64) << std::setfill('0') << address;
  }

  RPCparams params;
  params.method = "eth_sendTransaction";
  params.data = "0x334c1176" + txidVal + addressString.str();
  params.to = std::move(commitContractAddress);

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
