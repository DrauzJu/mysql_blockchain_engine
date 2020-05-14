#include "ethereum.h"

// todo: implement

ByteData* Ethereum::get(TableName table, ByteData* key) {
  std::cout << "Getting data for table " << table << ", key " << key->data << std::endl;
  return nullptr;
}

int Ethereum::put(TableName table, ByteData* key, ByteData* value) {
  std::cout << "Putting data for table " << table << ", key " << key->data << " and value " << value->data << std::endl;
  return 0;
}

ByteData *Ethereum::getAllKeys(TableName table) {
  std::cout << "Getting all keys for table " << table << std::endl;
  return nullptr;
}
