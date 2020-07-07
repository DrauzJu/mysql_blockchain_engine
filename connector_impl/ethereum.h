#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include <iostream>
#include <string>
#include <regex>
#include <cassert>
#include <curl/curl.h>

#include "../connector.h"
#include "json.hpp"

#define MINE_CHECK_TRIES 6

struct RPCparams {
  std::string from;
  std::string to;
  std::string data;
  std::string method;
  std::string gas;
  std::string gasPrice;
  std::string quantity_tag;
  std::string transactionID;
  int id;
  RPCparams() : id(1) {}
};


struct TransactionConfirmationException : public std::exception
{
  const char * what () const noexcept override
  {
    return "Transaction was not mined!";
  }
};


class Ethereum : public Connector {

public:
    explicit Ethereum(std::string connectionString,
                   std::string contractAddress, std::string fromAddress, int maxWaitingTime);
    ~Ethereum() override;

    int get(TableName table, ByteData* key, unsigned char* buf, int value_size) override;
    int put(TableName table, ByteData* key, ByteData* value) override;
    int putBatch(std::vector<PutOp>* data) override;
    int remove(TableName table, ByteData *key) override;
    void tableScanToVec(TableName table, std::vector<ManagedByteData> &tuples, size_t keyLength, size_t valueLength) override;
    void tableScanToMap(TableName table, tx_cache_t& tuples, size_t keyLength, size_t valueLength) override;
    int dropTable(TableName table) override;

    std::string call(const RPCparams params, bool setGas);
    std::string checkMiningResult(std::string transactionID);
    void updateMineCheckWaitingTime(int latestDurationMs);

private:
    std::string _contractAddress;
    std::string _fromAddress;
    std::string _connectionString;
    int maxWaitingTime;

    CURL *curl;

    int mineCheckWaitingTime[MINE_CHECK_TRIES] = {40000, 10000, 5000, 3000, 1000, 500};

    std::vector <std::string> tableScanCall();
    static size_t getTableScanResultsSize(std::vector<std::string> response);
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
