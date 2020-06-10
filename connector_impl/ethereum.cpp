#include "ethereum.h"
#include <include/my_base.h>
#include <iomanip>
#include <utility>

// todo: implement
// For document of methods see connector.h


void log(const std::string& msg, const std::string& method = "") {
    const std::string m = method.empty() ? "] " : "- " + method + "] ";
    std::cout << "[ETHEREUM" << m << msg << std::endl;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static void parse32ByteHexString(const std::string& s, uint8_t* out) {
    const char* hexString = s.c_str();

    for(int i=0; i<32; i++) {
        char byteString[3];
        byteString[0] = hexString[2*i];
        byteString[1] = hexString[2*i+1];
        byteString[2] = 0; // termination character

        // convert 1-byte hex string to numeric representation
        auto sNum = (uint8_t) strtoul(byteString, nullptr, 16);
        out[i] = sNum;
    }
}

static std::string byteArrayToHex(ByteData* data) {
    assert(data->dataSize <= 32);

    std::stringstream ss;
    ss << std::hex;
    int i=0;
    for (; i<data->dataSize; i++)
      ss << std::setw(2) << std::setfill('0') << (int) (data->data[i]);

    for(; i<32; i++)
      ss << std::setw(2) << std::setfill('0') << 0;

    return ss.str();
}

static std::vector<std::string> Split(const std::string& str, int splitLength) {
    int NumSubstrings = str.length() / splitLength;
    std::vector<std::string> ret;

    for (auto i = 0; i < NumSubstrings; i++) {
        ret.push_back(str.substr(i * splitLength, splitLength));
    }

    // If there are leftover characters, create a shorter item at the end.
    if (str.length() % splitLength != 0) {
        ret.push_back(str.substr(splitLength * NumSubstrings));
    }

    return ret;
}

static std::string parseParamsToJson(RPCparams params) {
    std::vector<std::string> els;
    std::string json = "";

    if (!params.from.empty()) els.push_back("\"from\":\"" + params.from + "\"");
    if (!params.data.empty()) els.push_back("\"data\":\"" + params.data + "\"");
    if (!params.to.empty()) els.push_back("\"to\":\"" + params.to + "\"");
    if (!params.gas.empty()) els.push_back("\"gas\":\"" + params.gas + "\"");
    if (!params.gasPrice.empty()) els.push_back("\"gasPrice\":\"" + params.gasPrice + "\"");

    for (std::vector<int>::size_type i = 0; i < els.size(); i++) {
        json += els[i];
        if (i < els.size() - 1) json += ",";
    }

    return json;
}





Ethereum::Ethereum(std::string connectionString,
                   std::string contractAddress, std::string fromAddress) {
    _contractAddress = std::move(contractAddress);
    _fromAddress = std::move(fromAddress);
    _connectionString = std::move(connectionString);

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L); // Problem: ganache default keep-alive timeout is only 5s
    curl_easy_setopt(curl, CURLOPT_URL, _connectionString.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    log("Contract Address: " + _contractAddress);
}

Ethereum::~Ethereum() {
    curl_easy_cleanup(curl);
}

int Ethereum::get(TableName, ByteData* key, unsigned char* buf, int value_size) {
  std::string hexKey = byteArrayToHex(key);

  RPCparams params;
  params.method = "eth_call";
  params.data = "0x8eaa6ac0" + hexKey;
  params.quantity_tag = "latest";
  log("Data: " + params.data, "Get");

  const std::string response = call(params, true);
  log("Response: " + response, "Get");


  if (response.find("error") == std::string::npos) {
    log("success", "Get");

    // extract result
    std::regex rgx(".*\"result\":\"0x(\\w+)\".*");
    std::smatch match;

    if (std::regex_search(response.begin(), response.end(), match, rgx)) {
      std::string result = match[1];

      uint8_t value[32]; // 32 byte value
      parse32ByteHexString(result, value);

      // Copy key
      memcpy(&(buf[0]), key->data, key->dataSize);

      // Copy value
      memcpy(&(buf[key->dataSize]), value, value_size);

      return 0;
    } else {
      log("No value for key found", "Get");
      return HA_ERR_END_OF_FILE;
    }
  } else {
    log("failed", "Get");
    return 1;
  }
}

int Ethereum::put(TableName, ByteData* key, ByteData* value) {

    std::string hexKey = byteArrayToHex(key);
    std::string hexVal = byteArrayToHex(value);


    RPCparams params;
    params.method = "eth_sendTransaction";
    params.data = "0x4c667080" + hexKey + hexVal;
    log("Data: " + params.data, "Put");

    const std::string response = call(params, true);
    log("Response: " + response, "Put");


    if (response.find("error") == std::string::npos) {
        log("success", "Put");
        return 0;
    } else {
        log("failed", "Put");
        return 1;
    }

}

int Ethereum::remove(TableName, ByteData *key) {

    std::string hexKey = byteArrayToHex(key);

    RPCparams params;
    params.method = "eth_sendTransaction";
    params.data = "0x95bc2673" + hexKey;
    log("Data: " + params.data, "Remove");

    const std::string response = call(params, true);
    log("Response: " + response, "Remove");


    if (response.find("error") == std::string::npos) {
        log("success", "Remove");
        return 0;
    } else {
        log("failed", "Remove");
        return 1;
    }

}

void Ethereum::tableScan(TableName, std::vector<ByteData>& tuples, size_t keyLength, size_t valueLength) {

    RPCparams params;
    params.method = "eth_call";
    params.data = "0xb3055e26";
    params.quantity_tag = "latest";


    std::regex rgx(".*\"result\":\"0x(\\w+)\".*");
    std::smatch match;

    const std::string s = call(params, false);
    log("Response: " + s, "tableScan");
    if (std::regex_search(s.begin(), s.end(), match, rgx)) {
        //std::cout << "match: " << match[1] << '\n';

        std::vector <std::string> results = Split(match[1], 64);

        unsigned int count;
        std::stringstream ss;
        ss << std::hex << results[2];
        ss >> count;

        for (std::vector<int>::size_type i = 3; i < 3 + count; i++) {

            int valueIndex = i + count + 1;

            uint8_t key[32]; // 32 byte key
            parse32ByteHexString(results[i], key);

            uint8_t value[32]; // 32 byte value
            parse32ByteHexString(results[valueIndex], value);

            auto row = new unsigned char[keyLength + valueLength];

            // Copy key
            memcpy(&(row[0]), key, keyLength);

            // Copy value
            memcpy(&(row[keyLength]), value, valueLength);

            ByteData bd = ByteData(row, keyLength + valueLength);
            tuples.push_back(bd);
        }

    } else log("no result in response", "tableScan");

}

int Ethereum::dropTable(TableName ) {
  // todo: implement: call function drop() of contract
  return 0;
}

std::string Ethereum::call(RPCparams params, bool setGas) {

  std::string readBuffer;

  params.from = _fromAddress;
  params.to = _contractAddress;
  if(setGas) params.gas = "0x7A120";
  // params.gasPrice = "0x4a817c800";

  const std::string json = parseParamsToJson(params);
  const std::string quantity_tag = params.quantity_tag.empty() ? "" : ",\"" + params.quantity_tag + "\"";
  const std::string postData = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(params.id) + ",\"method\":\"" + params.method + "\",\"params\":[{" + json + "}" + quantity_tag + "]}";
  log("Body: " + postData, "Call");

  if (curl) {
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_perform(curl);

    //std::cout << readBuffer << std::endl;
  } else log("no curl", "Call");

  return readBuffer;
}
