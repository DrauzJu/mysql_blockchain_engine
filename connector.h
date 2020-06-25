#ifndef MYSQL_8_0_20_CONNECTOR_H
#define MYSQL_8_0_20_CONNECTOR_H

#include <cstdint>
#include <iostream>
#include <vector>
#include "blockchain_tx.h"

/*
 * Interface definition to be used by storage engine to communicate with
 * concrete blockchain technology handler, like Ethereum
 */

class Connector {
 public:
  virtual ~Connector() {}

  /*
   * Write single KV-pair (concatenated) into byte buffer buf, starting at buf_write_index
   *
   */
  virtual int get(TableName table, ByteData* key, unsigned char* buf, int value_size) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int put(TableName table, ByteData* key, ByteData* value) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int putBatch(std::vector<std::unique_ptr<PutOp>> * data) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int remove(TableName table, ByteData* key) = 0;

  /*
   * Do a table scan, puts tuples in provided vector object (key+value concatenated)
   * --> faster than getting each KV-pair in an own transaction
   */
  virtual void tableScan(TableName table, std::vector<ByteData> &tuples, size_t keyLength, size_t valueLength) = 0;

  /*
   * Drop table
   */
  virtual int dropTable(TableName table) = 0;
};

#endif  // MYSQL_8_0_20_CONNECTOR_H
