#include "ethereum.h"

// todo: implement
// For document of methods see connector.h

Ethereum::Ethereum(const std::string&) {}

Ethereum::~Ethereum() = default;

int Ethereum::get(TableName, ByteData*, unsigned char*, int) {
  return 0;
}

int Ethereum::put(TableName, ByteData*, ByteData* ) {
  return 0;
}

int Ethereum::remove(TableName, ByteData *) {
  return 0;
}

void Ethereum::tableScan(TableName, std::vector<ByteData>) {}
