#ifndef MYSQL_HABC_TYPES_H
#define MYSQL_HABC_TYPES_H

// boost fix: https://github.com/boostorg/config/issues/322
#ifndef __clang_major__
#define __clang_major___WORKAROUND_GUARD 1
#else
#define __clang_major___WORKAROUND_GUARD 0
#endif

#include <iostream>
#include <unordered_map>
#include <boost/functional/hash.hpp>

using TableName = const char*;
using byte = unsigned char;

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
  ByteData() = default;
  ByteData(byte* p_data, uint8_t p_dataSize) {
    data = p_data;
    dataSize = p_dataSize;
  }

  byte* data;
  uint8_t dataSize;
};

/*
 * Should be used if data was allocated by storage engine (and not by MySQL core)
 * --> frees the data!
 */
class ManagedByteData {
 public:
  ManagedByteData() = default;
  explicit ManagedByteData(std::shared_ptr<std::vector<byte>>& p_data) {
    data = p_data;
  }
  explicit ManagedByteData(size_t size) {
    data = std::make_shared<std::vector<byte>>(size);
  }

  ManagedByteData(const ManagedByteData* key, ManagedByteData* value) {
    data = std::make_shared<std::vector<byte>>();
    data->reserve(key->data->size() + value->data->size());

    data->insert( data->end(), key->data->begin(), key->data->end());
    data->insert( data->end(), value->data->begin(), value->data->end());
  }

  std::shared_ptr<std::vector<byte>> data;
};

inline bool operator==(const ManagedByteData& lhs, const ManagedByteData& rhs) {
  return *(lhs.data.get()) == *(rhs.data.get());
}

// Define hash function for ManagedByteData
namespace std {
  template <>
  struct hash<ManagedByteData>
  {
    std::size_t operator()(const ManagedByteData& k) const
    {
      return boost::hash<std::vector<unsigned char>>()(*(k.data.get()));
    }
  };
}

class PutOp {
 public:
  TableName table{};
  ManagedByteData value;
  ManagedByteData key;
};


class RemoveOp {
 public:
  TableName table{};
  ManagedByteData key;
};

#endif  // MYSQL_HABC_TYPES_H
