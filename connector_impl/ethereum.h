#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include <curl/curl.h>
#include <storage/blockchain/connector.h>
#include <storage/blockchain/blockchain_table_tx.h>
#include <storage/blockchain/types.h>
#include <cassert>
#include <iostream>
#include <regex>
#include <string>

#include "json.hpp"

#define MINING_CHECK_INTERVAL 200

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
                   std::string storeContractAddress,
                   std::string fromAddress,
                   int maxWaitingTime);
    ~Ethereum() override;

    int get(ByteData* key, unsigned char* buf, int value_size) override;
    int put(ByteData* key, ByteData* value, TXID txID) override;
    int putBatch(std::vector<PutOp> * data, TXID txID) override;
    int remove(ByteData *key, TXID txID) override;
    void tableScanToVec(std::vector<ManagedByteData> &tuples, size_t keyLength, size_t valueLength) override;
    void tableScanToMap(tx_cache_t& tuples, size_t keyLength, size_t valueLength) override;
    int dropTable() override;
    int clearCommitPrepare(boost::uuids::uuid txID) override;

    std::string call(RPCparams params, bool setGas);
    std::string checkMiningResult(std::string transactionID);
    static int atomicCommit(std::string commitContractAddress, TXID txID, std::vector<std::string> addresses);

   private:
    std::string _storeContractAddress;
    std::string _fromAddress;
    std::string _connectionString;
    size_t maxWaitingTime;
    CURL *curl;

    std::vector <std::string> tableScanCall();
    static size_t getTableScanResultsSize(std::vector<std::string> response);
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
