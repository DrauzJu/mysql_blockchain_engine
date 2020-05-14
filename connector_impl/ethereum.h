#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include "../connector.h"

class Ethereum : public Connector {
 public:
  int get(TableName table, ByteData* key, unsigned char* buf, int buf_write_index) override;
  int put(TableName table, ByteData* key, ByteData* value) override;
  ByteData *getAllKeys(TableName table) override;
  ByteData *tableScan(TableName table) override;
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
