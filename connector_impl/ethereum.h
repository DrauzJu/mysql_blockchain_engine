#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include "../connector.h"

class Ethereum : public Connector {
 public:
  ByteData* get(ByteData* key);
  int put(ByteData* key, ByteData* value);
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
