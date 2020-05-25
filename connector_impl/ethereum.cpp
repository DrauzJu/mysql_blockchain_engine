#include "ethereum.h"

#include <iostream>
#include <string>
#include <curl/curl.h>


// todo: implement
// For document of methods see connector.h

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Ethereum::Ethereum(const std::string&) {}

Ethereum::~Ethereum() = default;

int Ethereum::get(TableName, ByteData*, unsigned char*, int) {
  return 0;
}

int Ethereum::put(TableName, ByteData*, ByteData* ) {
  return 0;
}

int Ethereum::remove(TableName, ByteData *) {
  return 0;
}

void Ethereum::tableScan(TableName, std::vector<ByteData> tuples) {
    CURL *curl;
    //CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8545");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",\"params\":[{\"to\":\"0x0E2A93B4cE39BDCb63687DCcB86C19f3E39b4a8e\"}, \"latest\"],\"id\":1}");
        /*res = */curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::cout << readBuffer << std::endl;
    } else std::cout << "no curl" << std::endl;
    tuples = tuples;

    tuples = std::vector<ByteData>();
}
