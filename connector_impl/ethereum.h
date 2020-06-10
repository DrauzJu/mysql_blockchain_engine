#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include <iostream>
#include <string>
#include <regex>
#include <cassert>
#include <curl/curl.h>

#include "../connector.h"
#include "json.hpp"

struct RPCparams {
  std::string from;
  std::string to;
  std::string data;
  std::string method;
  std::string gas;
  std::string gasPrice;
  std::string quantity_tag;
  int id;
  RPCparams() : id(1) {}
};

class Ethereum : public Connector {

public:
    explicit Ethereum(std::string connectionString,
                   std::string contractAddress, std::string fromAddress);
    ~Ethereum() override;

    int get(TableName table, ByteData* key, unsigned char* buf, int value_size) override;
    int put(TableName table, ByteData* key, ByteData* value) override;
    int remove(TableName table, ByteData *key) override;
    void tableScan(TableName table, std::vector<ByteData> &tuples, size_t keyLength, size_t valueLength) override;
    int dropTable(TableName table) override;

    std::string call(const RPCparams params, bool setGas);

private:
    std::string _contractAddress;
    std::string _fromAddress;
    std::string _connectionString;

    CURL *curl;
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
