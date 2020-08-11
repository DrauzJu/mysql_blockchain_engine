#ifndef MYSQL_HABC_TYPES_H
#define MYSQL_HABC_TYPES_H

// boost fix: https://github.com/boostorg/config/issues/322
#ifndef __clang_major__
#define __clang_major___WORKAROUND_GUARD 1
#else
#define __clang_major___WORKAROUND_GUARD 0
#endif
#define BOOST_FT_CC_IMPLICIT 0
#define BOOST_FT_CC_CDECL 0
#define BOOST_FT_CC_STDCALL 0
#define BOOST_FT_CC_PASCAL 0
#define BOOST_FT_CC_FASTCALL 0
#define BOOST_FT_CC_CLRCALL 0
#define BOOST_FT_CC_THISCALL 0
#define BOOST_FT_CC_IMPLICIT_THISCALL 0

#include <iostream>
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

using Table_name = std::string;
using byte = unsigned char;
using TXID = boost::uuids::uuid;

class Connector;
class blockchain_table_tx;

enum BC_TYPE {
  ETHEREUM = 0
};

typedef struct bc_ha_data_table_t {
  std::unique_ptr<blockchain_table_tx> tx;
  Connector* connector;
} bc_ha_data_table_t;

using ha_data_map = std::unordered_map<Table_name, std::unique_ptr<bc_ha_data_table_t>>;

class Byte_data {
 public:
  Byte_data() = default;
  Byte_data(byte* p_data, uint8_t p_dataSize) {
    data = p_data;
    data_size = p_dataSize;
  }

  byte* data;
  uint8_t data_size;
};

/*
 * Should be used if data was allocated by storage engine (and not by MySQL core)
 * --> frees the data!
 */
class Managed_byte_data {
 public:
  Managed_byte_data() = default;
  explicit Managed_byte_data(std::shared_ptr<std::vector<byte>>& p_data) {
    data = p_data;
  }
  explicit Managed_byte_data(size_t size) {
    data = std::make_shared<std::vector<byte>>(size);
  }

  std::shared_ptr<std::vector<byte>> data;
};

inline bool operator==(const Managed_byte_data& lhs, const Managed_byte_data& rhs) {
  return *(lhs.data.get()) == *(rhs.data.get());
}

// todo: evaluate use of another data structure (like <tsl/hopscotch_map.h>)
// requirements: allows random access and access by key
using tx_cache_t = std::unordered_map<Managed_byte_data, Managed_byte_data>;

// Define hash function for ManagedByteData
namespace std {
  template <>
  struct hash<Managed_byte_data>
  {
    std::size_t operator()(const Managed_byte_data& k) const
    {
      return boost::hash<std::vector<unsigned char>>()(*(k.data.get()));
    }
  };
}

class Put_op {
 public:
  Table_name table{};
  Managed_byte_data value;
  Managed_byte_data key;
};


class Remove_op {
 public:
  Table_name table{};
  Managed_byte_data key;
};

#endif  // MYSQL_HABC_TYPES_H
