#ifndef MYSQL_8_0_20_CONNECTOR_H
#define MYSQL_8_0_20_CONNECTOR_H

#include <cstdint>
#include <iostream>

/*
 * Interface definition to be used by storage engine to communicate with
 * concrete blockchain technology handler, like Ethereum
 *
 * Basic methods:
 *    - get(key)
 *    - put(key, value)
 */

class ByteData {
 public:
  unsigned char *data;
  uint8_t dataSize;
};

class Connector {
 public:
  virtual ByteData* get(ByteData* key) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int put(ByteData* key, ByteData* value) = 0;
};

#endif  // MYSQL_8_0_20_CONNECTOR_H
