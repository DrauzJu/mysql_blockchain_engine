#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include <iostream>
#include <string>
#include <regex>
#include <curl/curl.h>

#include "../connector.h"

class Ethereum : public Connector {

public:
    explicit Ethereum(std::string contractAddress);
    ~Ethereum() override;

    int get(TableName table, ByteData* key, unsigned char* buf, int buf_write_index) override;
    int put(TableName table, ByteData* key, ByteData* value) override;
    int remove(TableName table, ByteData *key) override;
    void tableScan(TableName table, std::vector<ByteData> &tuples, size_t keyLength, size_t valueLength) override;
    int dropTable(TableName table) override;

private:
    std::string _contractAddress;

};

#endif  // MYSQL_8_0_20_ETHEREUM_H
