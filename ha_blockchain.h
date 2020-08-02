/* Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file ha_blockchain.h

    @brief
  The ha_blockchain engine is a stubbed storage engine for blockchain purposes only;
  it does nothing at this point. Its purpose is to provide a source
  code illustration of how to begin writing new storage engines; see also
  /storage/blockchain/ha_blockchain.cc.

    @note
  Please read ha_blockchain.cc before reading this file.
  Reminder: The blockchain storage engine implements all methods that are
  *required* to be implemented. For a full list of all methods that you can
  implement, see handler.h.

   @see
  /sql/handler.h and /storage/blockchain/ha_blockchain.cc
*/

#include <sys/types.h>
#include <mutex>

#include "my_base.h" /* ha_rows */
#include "my_compiler.h"
#include "my_inttypes.h"
#include "sql/handler.h" /* handler */
#include "thr_lock.h"    /* THR_LOCK, THR_LOCK_DATA */

#include "blockchain_table_tx.h"
#include "connector.h"

/** @brief
  Class definition for the storage engine
*/
class ha_blockchain : public handler {
  my_off_t current_position; // current position during table scan
  std::unique_ptr<Connector> connector;
  std::vector<ManagedByteData> rndTableScanData;
  static std::mutex ha_data_create_mtx;
  static std::mutex ha_data_create_tx_mtx;

 public:
  // Maps table name to contract address
  static std::unordered_map<std::string, std::string>* tableContractInfo;

  ha_blockchain(handlerton *hton, TABLE_SHARE *table_arg);
  ~ha_blockchain();

  /** @brief
    The name that will be used for display purposes.
   */
  const char *table_type() const { return "BLOCKCHAIN"; }

  /**
    Replace key algorithm with one supported by SE, return the default key
    algorithm for SE if explicit key algorithm was not provided.

    @sa handler::adjust_index_algorithm().
  */
  virtual enum ha_key_alg get_default_index_algorithm() const {
    return HA_KEY_ALG_HASH;
  }
  virtual bool is_index_algorithm_supported(enum ha_key_alg key_alg) const {
    return key_alg == HA_KEY_ALG_HASH;
  }

  /** @brief
    This is a list of flags that indicate what functionality the storage engine
    implements. The current table flags are documented in handler.h
  */
  ulonglong table_flags() const {
    /*
      We are saying that this engine is just statement capable to have
      an engine that can only handle statement-based logging. This is
      used in testing.
    */
    return HA_BINLOG_STMT_CAPABLE;
  }

  /** @brief
    This is a bitmap of flags that indicates how the storage engine
    implements indexes. The current index flags are documented in
    handler.h. If you do not implement indexes, just return zero here.

      @details
    part is the key part to check. First key part is 0.
    If all_parts is set, MySQL wants to know the flags for the combined
    index, up to and including 'part'.
  */
  ulong index_flags(uint inx MY_ATTRIBUTE((unused)),
                    uint part MY_ATTRIBUTE((unused)),
                    bool all_parts MY_ATTRIBUTE((unused))) const {
    return 0;
  }

  /** @brief
    unireg.cc will call max_supported_record_length(), max_supported_keys(),
    max_supported_key_parts(), uint max_supported_key_length()
    to make sure that the storage engine can handle the data it is about to
    send. Return *real* limits of your storage engine here; MySQL will do
    min(your_limits, MySQL_limits) automatically.
   */
  uint max_supported_record_length() const { return 200; } // todo: 64 does not work, check why

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_keys() const { return 1; }

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_key_parts() const { return 1; }

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_key_length() const { return 32; }

  /** @brief
    Called in test_quick_select to determine if indexes should be used.
  */
  virtual double scan_time() {
    return (double)(stats.records + stats.deleted) / 20.0 + 10;
  }

  /** @brief
    This method will never be called if you do not implement indexes.
  */
  virtual double read_time(uint, uint, ha_rows rows) {
    return (double)rows / 20.0 + 1;
  }

  /*
    Everything below are methods that we implement in ha_blockchain.cc.

    Most of these methods are not obligatory, skip them and
    MySQL will treat them as not implemented
  */
  /** @brief
    We implement this in ha_blockchain.cc; it's a required method.
  */
  int open(const char *name, int mode, uint test_if_locked,
           const dd::Table *table_def);  // required

  /** @brief
    We implement this in ha_blockchain.cc; it's a required method.
  */
  int close(void);  // required

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int write_row(uchar *buf);

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int update_row(const uchar *old_data, uchar *new_data);

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int delete_row(const uchar *buf);

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_read_map(uchar *buf, const uchar *key, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);

  int index_read(uchar *buf, const uchar *key, uint key_len,
                 enum ha_rkey_function find_flag);
 public:
  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_next(uchar *buf);

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_prev(uchar *buf);

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_first(uchar *buf);

  /** @brief
    We implement this in ha_blockchain.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_last(uchar *buf);

  /** MySQL calls this function at the start of each SQL statement inside LOCK
  TABLES. Inside LOCK TABLES the "::external_lock" method does not work to mark
  SQL statement borders. Note also a special case: if a temporary table is
  created inside LOCK TABLES, MySQL has not called external_lock() at all on
  that table.
  MySQL-5.0 also calls this before each statement in an execution of a stored
  procedure. To make the execution more deterministic for binlogging, MySQL-5.0
  locks all tables involved in a stored procedure with full explicit table locks
  (thd_in_lock_tables(thd) holds in store_lock()) before executing the procedure.
  @param[in]	thd		handle to the user thread
  @param[in]	lock_type	lock type
  @return 0 or error code */
  int start_stmt(THD *thd, thr_lock_type lock_type);

  /** @brief
   * Logging helper
   */
  inline void log(const std::string& msg);
  inline void log(const std::string& msg, const char* tableName);

  void findConnector(const char* tableName);

  int find_current_row(uchar *buf);
  int find_row(my_off_t index, uchar *buf);

  void extract_key(uchar* buf, ByteData* key);
  void extract_value(uchar* buf, ulong key_size, ByteData* value);

  static std::unordered_map<std::string, std::string>* parseEthContractConfig(char* config);

  static bc_ha_data_table_t* ha_data_get(THD* thd, TableName table);
  static ha_data_map* ha_data_get_all(THD* thd);

  bool inTransaction();
  bool useTableScanCache();

  /** Commits a transaction or marks an SQL statement ended.*/
  int start_transaction(THD *thd);
  static int bc_commit(handlerton *hton, THD *thd, bool commit_trx);
  static int bc_rollback(handlerton *hton, THD *thd, bool all);

  /** @brief
    Unlike index_init(), rnd_init() can be called two consecutive times
    without rnd_end() in between (it only makes sense if scan=1). In this
    case, the second call should prepare for the new table scan (e.g if
    rnd_init() allocates the cursor, the second call should position the
    cursor to the start of the table; no need to deallocate and allocate
    it again. This is a required method.
  */
  int rnd_init(bool scan);  // required
  int rnd_end();
  int rnd_next(uchar *buf);             ///< required
  int rnd_pos(uchar *buf, uchar *pos);  ///< required
  void position(const uchar *record);   ///< required
  int info(uint);                       ///< required
  int extra(enum ha_extra_function operation);
  int external_lock(THD *thd, int lock_type);  ///< required
  int delete_all_rows(void);
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int delete_table(const char *from, const dd::Table *table_def);
  int rename_table(const char *from, const char *to,
                   const dd::Table *from_table_def, dd::Table *to_table_def);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info,
             dd::Table *table_def);  ///< required

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);  ///< required
};
