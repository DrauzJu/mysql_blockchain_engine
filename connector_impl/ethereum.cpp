#include "ethereum.h"

// todo: implement

ByteData* Ethereum::get(ByteData* key) {
  std::cout << "Getting data for key " << key->data << std::endl;
  return nullptr;
}

int Ethereum::put(ByteData* key, ByteData* value) {
  std::cout << "Putting data for key " << key->data << " and value " << value->data << std::endl;
  return 0;
}
