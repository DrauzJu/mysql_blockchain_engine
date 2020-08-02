#ifndef MYSQL_8_0_20_CONNECTOR_H
#define MYSQL_8_0_20_CONNECTOR_H

#include <cstdint>
#include <iostream>
#include <vector>
#include "types.h"

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
  virtual int get(ByteData* key, unsigned char* buf, int value_size) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int put(ByteData* key, ByteData* value, TXID txID = {{0}}) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int putBatch(std::vector<PutOp> * data, TXID txID = {{0}}) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int remove(ByteData* key, TXID txID = {{0}}) = 0;

  /*
   * Do a table scan, puts tuples in provided vector object (key+value concatenated)
   * --> faster than getting each KV-pair in an own transaction
   */
  virtual void tableScanToVec(std::vector<ManagedByteData> &tuples, const size_t keyLength, const size_t valueLength) = 0;

  /*
   * Do a table scan, puts tuples in provided map object (map key to value)
   * --> faster than getting each KV-pair in an own transaction
   */
  virtual void tableScanToMap(tx_cache_t& tuples, size_t keyLength, size_t valueLength) = 0;

  /*
   * Drop table
   */
  virtual int dropTable() = 0;

  /*
   * Clear commit prepare buffer
   */
  virtual int clearCommitPrepare(boost::uuids::uuid txID) = 0;
};

#endif  // MYSQL_8_0_20_CONNECTOR_H
