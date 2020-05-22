#include "ethereum.h"

// todo: implement
// For document of methods see connector.h

Ethereum::Ethereum(std::string) {}

int Ethereum::get(TableName, ByteData*, unsigned char*, int) {
  return 0;
}

int Ethereum::put(TableName, ByteData*, ByteData* ) {
  return 0;
}

int Ethereum::remove(TableName, ByteData *) {
  return 0;
}

ByteData *Ethereum::getAllKeys(TableName) {
  return nullptr;
}

ByteData *Ethereum::tableScan(TableName) {
  return nullptr;
}
