#ifndef MYSQL_TYPES_H
#define MYSQL_TYPES_H

#include <iostream>
#include <unordered_map>

using TableName = const char*;

class Connector;
class blockchain_table_tx;

enum BC_TYPE {
  ETHEREUM = 0
};

enum BC_ISOLATION_LEVEL {
  READ_UNCOMMITTED = 0,
  READ_COMMITTED = 1
};

typedef struct bc_ha_data_table_t {
  std::unique_ptr<blockchain_table_tx> tx;
  Connector* connector;
} bc_ha_data_table_t;

using ha_data_map = std::unordered_map<TableName, std::unique_ptr<bc_ha_data_table_t>>;

class ByteData {
 public:
  ByteData() {}
  ByteData(unsigned char *p_data, uint8_t p_dataSize) {
    data = p_data;
    dataSize = p_dataSize;
  }

  unsigned char *data;
  uint8_t dataSize;
};

/*
 * Should be used if data was allocated by storage engine (and not by MySQL core)
 * --> frees the data!
 */
class ManagedByteData {
 public:
  ManagedByteData() {}
  ManagedByteData(std::unique_ptr<unsigned char[]>& p_data, uint8_t p_dataSize) {
    data = std::move(p_data);
    dataSize = p_dataSize;
  }

  std::unique_ptr<unsigned char[]> data;
  uint8_t dataSize;
};

class PutOp {
 public:
  TableName table;
  ManagedByteData value;
  ManagedByteData key;
};


class RemoveOp {
 public:
  TableName table;
  ManagedByteData key;
};

#endif  // MYSQL_TYPES_H
