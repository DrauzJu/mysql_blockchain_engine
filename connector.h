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
   * Write single KV-pair (concatenated) into byte buffer buf
   *
   */
  virtual int get(Byte_data* key, unsigned char* buf, int value_size) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int put(Byte_data* key, Byte_data* value, TXID txID = {{0}}) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int put_batch(std::vector<Put_op> * data, TXID txID = {{0}}) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int remove(Byte_data* key, TXID txID = {{0}}) = 0;

  /*
   * returns 0 on success, 1 on failure
   */
  virtual int remove_batch(std::vector<Remove_op> * data, TXID txID = {{0}}) = 0;

  /*
   * Do a table scan, puts tuples in provided vector object (key+value concatenated)
   * --> faster than getting each KV-pair in an own transaction
   */
  virtual void table_scan_to_vec(std::vector<Managed_byte_data> &tuples, const size_t keyLength, const size_t valueLength) = 0;

  /*
   * Do a table scan, puts tuples in provided map object (map key to value)
   * --> faster than getting each KV-pair in an own transaction
   */
  virtual void table_scan_to_map(tx_cache_t& tuples, size_t keyLength, size_t valueLength) = 0;

  /*
   * Drop table
   */
  virtual int drop_table() = 0;

  /*
   * Clear commit prepare buffer
   */
  virtual int clear_commit_prepare(boost::uuids::uuid txID) = 0;
};

#endif  // MYSQL_8_0_20_CONNECTOR_H
