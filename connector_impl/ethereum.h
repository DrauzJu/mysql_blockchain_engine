#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include "../connector.h"

class Ethereum : public Connector {
 public:
  explicit Ethereum(const std::string& contractAddress);
  ~Ethereum() override;

  int get(TableName table, ByteData* key, unsigned char* buf, int buf_write_index) override;
  int put(TableName table, ByteData* key, ByteData* value) override;
  int remove(TableName table, ByteData *key) override;
  void tableScan(TableName table, std::vector<ByteData> tuples) override;
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
