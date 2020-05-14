#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include "../connector.h"

class Ethereum : public Connector {
 public:
  ByteData* get(TableName table, ByteData* key) override;
  int put(TableName table, ByteData* key, ByteData* value) override;
  ByteData *getAllKeys(TableName table) override;
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
