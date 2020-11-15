/* Copyright (c) 2004, 2020, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file ha_blockchain.cc

  @brief
  The ha_blockchain engine is a stubbed storage engine for blockchain purposes only;
  it does nothing at this point. Its purpose is to provide a source
  code illustration of how to begin writing new storage engines; see also
  /storage/blockchain/ha_blockchain.h.

  @details
  ha_blockchain will let you create/open/delete tables, but
  nothing further (for blockchain, indexes are not supported nor can data
  be stored in the table). Use this blockchain as a template for
  implementing the same functionality in your own storage engine. You
  can enable the blockchain storage engine in your build by doing the
  following during your build process:<br> ./configure
  --with-blockchain-storage-engine

  Once this is done, MySQL will let you create tables with:<br>
  CREATE TABLE \<table name\> (...) ENGINE=BLOCKCHAIN;

  The blockchain storage engine is set up to use table locks. It
  implements an example "SHARE" that is inserted into a hash by table
  name. You can use this to store information of state that any
  blockchain handler object will be able to see when it is using that
  table.

  Please read the object definition in ha_blockchain.h before reading the rest
  of this file.

  @note
  When you create an BLOCKCHAIN table, the MySQL Server creates a table .frm
  (format) file in the database directory, using the table name as the file
  name as is customary with MySQL. No other files are created. To get an idea
  of what occurs, here is an example select that would do a scan of an entire
  table:

  @code
  ha_blockchain::store_lock
  ha_blockchain::external_lock
  ha_blockchain::info
  ha_blockchain::rnd_init
  ha_blockchain::extra
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::rnd_next
  ha_blockchain::extra
  ha_blockchain::external_lock
  ha_blockchain::extra
  ENUM HA_EXTRA_RESET        Reset database to after open
  @endcode

  Here you see that the blockchain storage engine has 9 rows called before
  rnd_next signals that it has reached the end of its data. Also note that
  the table in question was already opened; had it not been open, a call to
  ha_blockchain::open() would also have been necessary. Calls to
  ha_blockchain::extra() are hints as to what will be occurring to the request.

  A Longer Example can be found called the "Skeleton Engine" which can be
  found on TangentOrg. It has both an engine and a full build environment
  for building a pluggable storage engine.

  Happy coding!<br>
    -Brian
*/

#include "storage/blockchain/ha_blockchain.h"
#include <sql/sql_thd_internal_api.h>
#include <sql/table.h>
#include <iostream>
#include <vector>

#include "my_dbug.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/plugin.h"
#include "sql/field.h"
#include "sql/sql_class.h"
#include "sql/sql_plugin.h"
#include "typelib.h"

#include "connector_impl/ethereum.h"

static handler *blockchain_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool partitioned, MEM_ROOT *mem_root);

handlerton *blockchain_hton;

// System variables for configuration
static int config_type;
static char* config_connection;
static int config_use_ts_cache;
static int config_tx_prepare_immediately;
static char* config_eth_contracts;
static char* config_eth_tx_contract;
static char* config_eth_from;
static int config_eth_max_waiting_time;

/* Interface to mysqld, to check system tables supported by SE */
static bool blockchain_is_supported_system_table(const char *db,
                                              const char *table_name,
                                              bool is_sql_layer_system_table);

static int blockchain_init_func(void *p) {
  DBUG_TRACE;

  blockchain_hton = (handlerton *)p;
  blockchain_hton->state = SHOW_OPTION_YES;
  blockchain_hton->create = blockchain_create_handler;
  blockchain_hton->flags = HTON_CAN_RECREATE | HTON_ALTER_NOT_SUPPORTED;
  blockchain_hton->is_supported_system_table = blockchain_is_supported_system_table;
  blockchain_hton->commit = ha_blockchain::bc_commit;
  blockchain_hton->rollback = ha_blockchain::bc_rollback;
  blockchain_hton->close_connection = ha_blockchain::bc_close_connection;

  // Comment: blockchain_hton->prepare is not set 
  // --> Two-Phase Commit is not supported by this storage engine

  // Parse configuration
  if(config_type == ETHEREUM) {
    ha_blockchain::table_contract_info =
        ha_blockchain::parse_eth_contract_config(config_eth_contracts);
  }

  return 0;
}

static handler *blockchain_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_blockchain(hton, table);
}

// Create static members
std::unordered_map<Table_name, std::string>* ha_blockchain::table_contract_info;
std::mutex ha_blockchain::ha_data_create_tx_mtx;
std::atomic_uint64_t Ethereum::nonce;
std::mutex Ethereum::nonce_init_mtx;

ha_blockchain::ha_blockchain(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {
  // ensure connection-scoped data structures are initialized
  init_HAData(ha_thd());
}

ha_blockchain::~ha_blockchain() = default;

/*
  List of all system tables specific to the SE.
  Array element would look like below,
     { "<database_name>", "<system table name>" },
  The last element MUST be,
     { (const char*)NULL, (const char*)NULL }

  This array is optional, so every SE need not implement it.
*/
static st_handler_tablename ha_blockchain_system_tables[] = {
    {(const char *)nullptr, (const char *)nullptr}};

/**
  @brief Check if the given db.tablename is a system table for this SE.

  @param db                         Database name to check.
  @param table_name                 table name to check.
  @param is_sql_layer_system_table  if the supplied db.table_name is a SQL
                                    layer system table.

  @retval true   Given db.table_name is supported system table.
  @retval false  Given db.table_name is not a supported system table.
*/
static bool blockchain_is_supported_system_table(const char *db,
                                              const char *table_name,
                                              bool is_sql_layer_system_table) {
  st_handler_tablename *systab;

  // Does this SE support "ALL" SQL layer system tables ?
  if (is_sql_layer_system_table) return false;

  // Check if this is SE layer system tables
  systab = ha_blockchain_system_tables;
  while (systab && systab->db) {
    if (systab->db == db && strcmp(systab->tablename, table_name) == 0)
      return true;
    systab++;
  }

  return false;
}

/**
  @brief
  Used for opening tables. The name will be the name of the file.

  @details
  A table is opened when it needs to be opened; e.g. when a request comes in
  for a SELECT on the table (tables are not open and closed for each request,
  they are cached).

  Called from handler.cc by handler::ha_open(). The server opens all tables by
  calling ha_open() which then calls the handler specific open().

  @see
  handler::ha_open() in handler.cc
*/

int ha_blockchain::open(const char *full_table_name, int, uint, const dd::Table *) {
  DBUG_TRACE;
  log("Opening table");

  const char* table_name = table ? table->alias : full_table_name;
  find_connector(table_name);
  
  return 0;
}

/**
  @brief
  Closes a table.

  @details
  Called from sql_base.cc, sql_select.cc, and table.cc. In sql_select.cc it is
  only used to close up temporary tables or during the process where a
  temporary table is converted over to being a myisam table.

  For sql_base.cc look at close_data_tables().

  @see
  sql_base.cc, sql_select.cc and table.cc
*/

int ha_blockchain::close() {
  DBUG_TRACE;
  return 0;
}

/**
  @brief
  write_row() inserts a row. No extra() hint is given currently if a bulk load
  is happening. buf() is a byte array of data. You can use the field
  information to extract the data from the native byte array type.

  @details
  Blockchain of this would be:
  @code
  for (Field **field=table->field ; *field ; field++)
  {
    ...
  }
  @endcode

  See ha_tina.cc for an example of extracting all of the data as strings.
  ha_berekly.cc has an example of how to store it intact by "packing" it
  for ha_berkeley's own native storage type.

  See the note for update_row() on auto_increments. This case also applies to
  write_row().

  Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.

  @see
  item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
  sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc and sql_update.cc
*/

int ha_blockchain::write_row(uchar *buf) {
  DBUG_TRACE;

  Byte_data key{}, value{};
  extract_key(buf, &key);
  extract_value(buf, key.data_size, &value);

  if(in_transaction()) {
    // copy data in heap and store pointer in blockchain_tx object

    Put_op put_op;
    put_op.value = Managed_byte_data(value.data_size);
    memcpy(put_op.value.data->data(), value.data, value.data_size);
    put_op.key = Managed_byte_data(key.data_size);
    memcpy(put_op.key.data->data(), key.data, key.data_size);

    Table_name table_name(table->alias);
    auto bc_thd_data = ha_data_get(ha_thd(), table_name);
    bc_thd_data->tx->add_put(std::move(put_op), connector.get());

    return 0;
  } else {
    // else (auto-commit): don't copy any data, just use buf to directly store data
    return connector->put(&key, &value);
  }
}

/**
  @brief
  Yes, update_row() does what you expect, it updates a row. old_data will have
  the previous row record in it, while new_data will have the newest data in it.
  Keep in mind that the server can do updates based on ordering if an ORDER BY
  clause was used. Consecutive ordering is not guaranteed.

  @details
  Currently new_data will not have an updated auto_increament record. You can
  do this for example by doing:

  @code

  if (table->next_number_field && record == table->record[0])
    update_auto_increment();

  @endcode

  Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.

  @see
  sql_select.cc, sql_acl.cc, sql_update.cc and sql_insert.cc
*/
int ha_blockchain::update_row(const uchar *old_data, uchar *new_data) {
  DBUG_TRACE;

  Byte_data key_old, key_new;
  extract_key(const_cast<uchar *>(old_data), &key_old);
  extract_key(new_data, &key_new);

  // Check if keys are still the same
  if(key_old.data_size == key_new.data_size) {
    if(memcmp(key_old.data, key_new.data, key_old.data_size) != 0) {
      return HA_ERR_WRONG_COMMAND; // key content is different
    }
  } else {
    return HA_ERR_WRONG_COMMAND; // key size is different
  }

  return write_row(new_data);
}

/**
  @brief
  This will delete a row. buf will contain a copy of the row to be deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_nexT() or index call).

  @details
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier. Keep in mind that the server does
  not guarantee consecutive deletions. ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table
  information.  Called in sql_delete.cc, sql_insert.cc, and
  sql_select.cc. In sql_select it is used for removing duplicates
  while in insert it is used for REPLACE calls.

  @see
  sql_acl.cc, sql_udf.cc, sql_delete.cc, sql_insert.cc and sql_select.cc
*/

int ha_blockchain::delete_row(const uchar *buf) {
  DBUG_TRACE;
  Byte_data key{};
  extract_key(const_cast<uchar *>(buf), &key);

  if(in_transaction()) {
    // copy data in heap and store pointer in blockchain_tx object

    Remove_op remove_op;

    remove_op.key = Managed_byte_data(key.data_size);
    memcpy(remove_op.key.data->data(), key.data, key.data_size);

    Table_name table_name(table->alias);
    auto bc_thd_data = ha_data_get(ha_thd(), table_name);

    // Only add pending remove
    // --> allows to further iterate over cache without invalidating iterator pointers
    bc_thd_data->tx->add_remove(std::move(remove_op), true, connector.get());

    return 0;
  } else {
    // else (auto-commit): don't copy any data, just use buf to directly store data
    return connector->remove(&key);
  }
}

/**
  @brief
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.
*/

int ha_blockchain::index_read_map(uchar *buf, const uchar *key, key_part_map,
                               enum ha_rkey_function func) {
  return index_read(buf, key, 0, func);
}

/**
  @brief
  Used to read forward through the index.
*/

int ha_blockchain::index_next(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  Used to read backwards through the index.
*/

int ha_blockchain::index_prev(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  index_first() asks for the first key in the index.

  @details
  Called from opt_range.cc, opt_sum.cc, sql_handler.cc, and sql_select.cc.

  @see
  opt_range.cc, opt_sum.cc, sql_handler.cc and sql_select.cc
*/
int ha_blockchain::index_first(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  index_last() asks for the last key in the index.

  @details
  Called from opt_range.cc, opt_sum.cc, sql_handler.cc, and sql_select.cc.

  @see
  opt_range.cc, opt_sum.cc, sql_handler.cc and sql_select.cc
*/
int ha_blockchain::index_last(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

int ha_blockchain::index_read(uchar *buf, const uchar *key, uint,
                              enum ha_rkey_function key_func) {
  find_connector(table->alias);                              

  // Check that exact match is required
  if(key_func != HA_READ_KEY_EXACT) {
    return HA_ERR_WRONG_COMMAND;
  }

  // check that used index uses first column
  KEY key_used = table->key_info[active_index];
  if(key_used.key_part->field != *(table->field)) {
    return HA_ERR_WRONG_COMMAND;
  }

  uint initial_null_bytes = table->s->null_bytes;
  uint8_t key_size = (*(table->field))->field_length;
  int value_size = table->s->reclength - key_size - initial_null_bytes;

  // set required zero bits
  memset(buf, 0, initial_null_bytes + key_size + value_size);
  uint pos = initial_null_bytes;

  // Extract key
  Byte_data key_BD(const_cast<uchar*>(key), key_size);

  if(in_transaction()) {
    Table_name table_name(table->alias);
    auto& tx = ha_data_get(ha_thd(), table_name)->tx;

    // Create temp key
    Managed_byte_data tmp_key(key_BD.data_size);
    memcpy(tmp_key.data->data(), key_BD.data, key_BD.data_size);

    /*Execute a GET if
      1. Table Scan Cache is not used
      2. Table Scan Cache is used but key does not exist

      + re-apply pending TX operations 
    */

    if(!use_table_scan_cache() || 
        tx->table_scan_data.find(tmp_key) == tx->table_scan_data.end()) {
          
      // Get single value
      Managed_byte_data tmp_tuple(key_size + value_size);
      auto get_rc = connector->get(&key_BD, tmp_tuple.data->data(), value_size);
      tmp_tuple.data->erase(tmp_tuple.data->begin(), tmp_tuple.data->begin() + key_size); // only keep value part
      if(get_rc == 0) {
        // Put value into cache
        tx->table_scan_data[tmp_key] = std::move(tmp_tuple);

        // Apply pending ops
        tx->reapply_pending_operations();
      } else {
        return 0;
      }
    }

    // Search, extract and copy value
    auto iter = tx->table_scan_data.find(tmp_key);
    if(iter != tx->table_scan_data.end()) { // Copy only if value was found
      // Copy key
      memcpy(&(buf[pos]), key_BD.data, key_BD.data_size);
      pos += key_BD.data_size;

      // Copy value
      memcpy(&buf[pos], iter->second.data->data(), iter->second.data->size());
    }

    if(!use_table_scan_cache()) {
      tx->table_scan_data.clear();
    }
  } else {
    connector->get(&key_BD, &(buf[pos]), value_size);
  }

  return 0;
}

/**
  @brief
  rnd_init() is called when the system wants the storage engine to do a table
  scan. See the example in the introduction at the top of this file to see when
  rnd_init() is called.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and
  sql_update.cc
*/
int ha_blockchain::rnd_init(bool) {
  Field* key_field = *(table->field);
  size_t key_length = key_field->field_length;
  size_t value_length = table->s->reclength - key_length - table->s->null_bytes;

  current_position = -1;

  if(in_transaction()) {
    Table_name table_name(table->alias);
    auto& tx = ha_data_get(ha_thd(), table_name)->tx;
    tx->pending_remove_activated = true;

    if(!use_table_scan_cache()) {
      tx->table_scan_data.clear();
      tx->table_scan_data_filled = false;
    }

    if(!tx->table_scan_data_filled) {
      connector->table_scan_to_map(tx->table_scan_data, key_length, value_length);
      tx->reapply_pending_operations();
      tx->table_scan_data_filled = true;
    }

  } else {
    connector->table_scan_to_vec(rnd_table_scan_data, key_length, value_length);
  }

  DBUG_TRACE;
  return 0;
}

int ha_blockchain::rnd_end() {
  DBUG_TRACE;

  if(!in_transaction()) {
    // just clear temporary data used for table scan
    rnd_table_scan_data.clear();
  } else {
    Table_name table_name(table->alias);
    auto& tx = ha_data_get(ha_thd(), table_name)->tx;
    tx->apply_pending_remove_ops(connector.get());
    tx->pending_remove_activated = false;

    if(!use_table_scan_cache()) {
      tx->table_scan_data.clear();
    }
  }

  return 0;
}

/**
  @brief
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and
  sql_update.cc
*/
int ha_blockchain::rnd_next(uchar *buf) {
  DBUG_TRACE;

  current_position++; // necessary to do before fetching row so that position() still works
  return find_current_row(buf);
}

/**
  @brief
  position() is called after each call to rnd_next() if the data needs
  to be ordered. You can do something like the following to store
  the position:
  @code
  my_store_ptr(ref, ref_length, current_position);
  @endcode

  @details
  The server uses ref to store data. ref_length in the above case is
  the size needed to store current_position. ref is just a byte array
  that the server will maintain. If you are using offsets to mark rows, then
  current_position should be the offset. If it is a primary key like in
  BDB, then it needs to be a primary key.

  Called from filesort.cc, sql_select.cc, sql_delete.cc, and sql_update.cc.

  @see
  filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc
*/
void ha_blockchain::position(const uchar *) {
  DBUG_TRACE;
  my_store_ptr(ref, ref_length, current_position);
}

/**
  @brief
  This is like rnd_next, but you are given a position to use
  to determine the row. The position will be of the type that you stored in
  ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
  or position you saved when position() was called.

  @details
  Called from filesort.cc, records.cc, sql_insert.cc, sql_select.cc, and
  sql_update.cc.

  @see
  filesort.cc, records.cc, sql_insert.cc, sql_select.cc and sql_update.cc
*/
int ha_blockchain::rnd_pos(uchar *buf, uchar *pos) {
  DBUG_TRACE;

  auto position = my_get_ptr(pos, ref_length);
  return find_row(position, buf);
}

/**
  @brief
  ::info() is used to return information to the optimizer. See my_base.h for
  the complete description.

  @details
  Currently this table handler doesn't implement most of the fields really
  needed. SHOW also makes use of this data.

  You will probably want to have the following in your code:
  @code
  if (records < 2)
    records = 2;
  @endcode
  The reason is that the server will optimize for cases of only a single
  record. If, in a table scan, you don't know the number of records, it
  will probably be better to set records to two so you can return as many
  records as you need. Along with records, a few more variables you may wish
  to set are:
    records
    deleted
    data_file_length
    index_file_length
    delete_length
    check_time
  Take a look at the public variables in handler.h for more information.

  Called in filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc,
  sql_delete.cc, sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_show.cc, sql_table.cc, sql_union.cc, and sql_update.cc.

  @see
  filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc,
  sql_delete.cc, sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_show.cc, sql_table.cc, sql_union.cc and sql_update.cc
*/
int ha_blockchain::info(uint) {
  DBUG_TRACE;
  stats.records = 10;
  return 0;
}

/**
  @brief
  extra() is called whenever the server wishes to send a hint to
  the storage engine. The myisam engine implements the most hints.
  ha_innodb.cc has the most exhaustive list of these hints.

    @see
  ha_innodb.cc
*/
int ha_blockchain::extra(enum ha_extra_function) {
  DBUG_TRACE;
  return 0;
}

/**
  @brief
  Used to delete all rows in a table, including cases of truncate and cases
  where the optimizer realizes that all rows will be removed as a result of an
  SQL statement.

  @details
  Called from item_sum.cc by Item_func_group_concat::clear(),
  Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
  Called from sql_delete.cc by mysql_delete().
  Called from sql_select.cc by JOIN::reinit().
  Called from sql_union.cc by st_select_lex_unit::exec().

  @see
  Item_func_group_concat::clear(), Item_sum_count_distinct::clear() and
  Item_func_group_concat::clear() in item_sum.cc;
  mysql_delete() in sql_delete.cc;
  JOIN::reinit() in sql_select.cc and
  st_select_lex_unit::exec() in sql_union.cc.
*/
int ha_blockchain::delete_all_rows() {
  DBUG_TRACE;
  return HA_ERR_WRONG_COMMAND;
}

/**
  @brief
  This create a lock on the table. If you are implementing a storage engine
  that can handle transacations look at ha_berkely.cc to see how you will
  want to go about doing this. Otherwise you should consider calling flock()
  here. Hint: Read the section "locking functions for mysql" in lock.cc to
  understand this.

  @details
  Called from lock.cc by lock_external() and unlock_external(). Also called
  from sql_table.cc by copy_data_between_tables().

  @see
  lock.cc by lock_external() and unlock_external() in lock.cc;
  the section "locking functions for mysql" in lock.cc;
  copy_data_between_tables() in sql_table.cc.
*/
int ha_blockchain::external_lock(THD *thd, int lock_type) {
  if(lock_type == F_UNLCK) {
    return 0;
  }

  return start_transaction(thd);
}

/**
  @brief
  The idea with handler::store_lock() is: The statement decides which locks
  should be needed for the table. For updates/deletes/inserts we get WRITE
  locks, for SELECT... we get read locks.

  @details
  Before adding the lock into the table lock handler (see thr_lock.c),
  mysqld calls store lock with the requested locks. Store lock can now
  modify a write lock to a read lock (or some other lock), ignore the
  lock (if we don't want to use MySQL table locks at all), or add locks
  for many tables (like we do when we are using a MERGE handler).

  Berkeley DB, for example, changes all WRITE locks to TL_WRITE_ALLOW_WRITE
  (which signals that we are doing WRITES, but are still allowing other
  readers and writers).

  When releasing locks, store_lock() is also called. In this case one
  usually doesn't have to do anything.

  In some exceptional cases MySQL may send a request for a TL_IGNORE;
  This means that we are requesting the same lock as last time and this
  should also be ignored. (This may happen when someone does a flush
  table when we have opened a part of the tables, in which case mysqld
  closes and reopens the tables and tries to get the same locks at last
  time). In the future we will probably try to remove this.

  Called from lock.cc by get_lock_data().

  @note
  In this method one should NEVER rely on table->in_use, it may, in fact,
  refer to a different thread! (this happens if get_lock_data() is called
  from mysql_lock_abort_for_thread() function)

  @see
  get_lock_data() in lock.cc
*/
THR_LOCK_DATA **ha_blockchain::store_lock(THD *, THR_LOCK_DATA **to,
                                       enum thr_lock_type) {
  // Blockchain: ignore all locks --> global order determined by blockchain
  // (*to)->type = TL_IGNORE;

  return to;
}

/**
  @brief
  Used to delete a table. By the time delete_table() has been called all
  opened references to this table will have been closed (and your globally
  shared references released). The variable name will just be the name of
  the table. You will need to remove any files you have created at this point.

  @details
  If you do not implement this, the default delete_table() is called from
  handler.cc and it will delete all files with the file extensions from
  handlerton::file_extensions.

  Called from handler.cc by delete_table and ha_create_table(). Only used
  during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
  the storage engine.

  @see
  delete_table and ha_create_table() in handler.cc
*/
int ha_blockchain::delete_table(const char *name, const dd::Table *) {
  find_connector(name);
  return connector->drop_table();
}

/**
  @brief
  Renames a table from one name to another via an alter table call.

  @details
  If you do not implement this, the default rename_table() is called from
  handler.cc and it will delete all files with the file extensions from
  handlerton::file_extensions.

  Called from sql_table.cc by mysql_rename_table().

  @see
  mysql_rename_table() in sql_table.cc
*/
int ha_blockchain::rename_table(const char *, const char *, const dd::Table *,
                             dd::Table *) {
  DBUG_TRACE;
  return HA_ERR_WRONG_COMMAND;
}

/**
  @brief
  Given a starting key and an ending key, estimate the number of rows that
  will exist between the two keys.

  @details
  end_key may be empty, in which case determine if start_key matches any rows.

  Called from opt_range.cc by check_quick_keys().

  @see
  check_quick_keys() in opt_range.cc
*/
ha_rows ha_blockchain::records_in_range(uint, key_range *, key_range *) {
  DBUG_TRACE;
  return 10;  // low number to force index usage
}

/**
  @brief
  create() is called to create a database. The variable name will have the name
  of the table.

  @details
  When create() is called you do not need to worry about
  opening the table. Also, the .frm file will have already been
  created so adjusting create_info is not necessary. You can overwrite
  the .frm file at this point if you wish to change the table
  definition, but there are no methods currently provided for doing
  so.

  Called from handle.cc by ha_create_table().

  @see
  ha_create_table() in handle.cc
*/

int ha_blockchain::create(const char *name, TABLE *, HA_CREATE_INFO *,
                       dd::Table *) {
  DBUG_TRACE;

  std::stringstream msg;
  msg << "Creating new table: " << name << std::endl;
  log(msg.str());

  return 0;
}


int ha_blockchain::start_stmt(THD *thd, thr_lock_type) {
  return start_transaction(thd);
}

int ha_blockchain::start_transaction(THD *thd) {
  // Check if transaction needs to be created
  if(!in_transaction()) {
    return 0;
  }

  Table_name table_name(table->alias);
  auto bc_ha_data = ha_data_get(thd, table_name);

  if(bc_ha_data->tx == nullptr) {
    std::lock_guard<std::mutex> lock(ha_data_create_tx_mtx);
    bc_ha_data->tx = std::make_unique<blockchain_table_tx>(thd,
                                                           blockchain_hton->slot,
                                                           config_tx_prepare_immediately);
    log("Creating transaction and registering for table " + table_name);

    // register transaction in MySQL core
    // See sql/handler.cc for details
    trans_register_ha(thd, true, blockchain_hton, nullptr);

    // If a transaction exists, enusre also that a corresponding connector object is available in THD data
    bc_ha_data->connector = connector.get();
  }

  return 0;
}

int ha_blockchain::bc_commit(handlerton *, THD *thd, bool commit_trx) {

  if(!commit_trx) {
    return HA_ERR_WRONG_COMMAND;
  }

  auto affected_tables = std::vector<Table_name>();
  TXID txID;

  // For each table that took part in transaction, prepare commit
  for(auto& table_data : *ha_data_get_all(thd)) {
    auto connector = table_data.second->connector;
    auto tx = std::move(table_data.second->tx);

    if(tx == nullptr) {
      // transaction did not touch this table --> continue
      continue;
    }

    if(tx->is_read_only()) {
      continue;
    }

    // Add table to list of affected tables
    affected_tables.emplace_back(table_data.first);
    txID = tx->get_ID();

    bool success_prepare = true;

    if(config_tx_prepare_immediately) {
      // Only wait until preparation is done
      success_prepare = tx->wait_for_commit_prepare_workers();
    } else {
      // Prepare call to write_batch
      if(!tx->get_put_operations()->empty()) {
        std::cout << "[BLOCKCHAIN] Preparing commit with " << tx->get_put_operations()->size() << " put operations" << std::endl;
        int rc_putBatch = connector->put_batch(tx->get_put_operations(), tx->get_ID());
        success_prepare = std::min(success_prepare, rc_putBatch == 0);
        tx->get_put_operations()->clear();
      }

      if(!tx->get_remove_operations()->empty()) {
        std::cout << "[BLOCKCHAIN] Preparing commit with " << tx->get_remove_operations()->size() << " remove operations" << std::endl;
        int rc_removeBatch = connector->remove_batch(tx->get_remove_operations(), tx->get_ID());
        success_prepare = std::min(success_prepare, rc_removeBatch == 0);
        tx->get_remove_operations()->clear();
      }
    }

    if(!success_prepare) {
      std::cerr << "Prepare of commit failed, will undo preparation of all involved tables. "
                << "Transaction is deleted, please create a new one! "
                << std::endl;
      auto allHAData = ha_data_get_all(thd);
      for(auto& table : affected_tables) {
        auto table_connector = (*allHAData)[table]->connector;
        table_connector->clear_commit_prepare(tx->get_ID());
      }

      // Notify MySQL core (see sql/handler.cc)
      thd->transaction_rollback_request = true;

      return HA_ERR_INTERNAL_ERROR;
    }
  }

  if(affected_tables.empty()) {
    return 0; // nothing to commit
  }

  // Preparation of commit was successful --> call commit contract with all
  // addresses to do atomic commit
  auto addresses = std::vector<std::string>(affected_tables.size());
  for(size_t i=0; i<affected_tables.size(); i++) {
    addresses[i] = (*ha_blockchain::table_contract_info)[affected_tables[i]];
  }

  switch (config_type) {
    case ETHEREUM: {
      return Ethereum::atomic_commit(std::string(config_connection),
                                    std::string(config_eth_from),
                                    config_eth_max_waiting_time,
                                    std::string(config_eth_tx_contract),
                                    txID, addresses);
    }
    default: return HA_ERR_WRONG_COMMAND;
  }
}

int ha_blockchain::bc_rollback(handlerton *, THD *thd, bool all) {

  if(!all) {
    return HA_ERR_WRONG_COMMAND;
  }

  // For each table that took part in transaction, delete pending operations
  for(auto& table_data : *ha_data_get_all(thd)) {
    auto tx = std::move(table_data.second->tx);

    if(tx == nullptr) {
      // transaction did not touch this table --> continue
      continue;
    }

    if(config_tx_prepare_immediately) {
      tx->wait_for_commit_prepare_workers(); // ensure threads shut down gracefully
      auto tableConnector = table_data.second->connector;
      tableConnector->clear_commit_prepare(tx->get_ID());
    }
  }

  return 0;
}

/* Custom functions */

void ha_blockchain::log(const std::string& msg) {
  if(table) {
    log(msg, table->alias);
  } else {
    log(msg, Table_name().c_str());
  }
}

void ha_blockchain::log(const std::string& msg, const char* table_name) {
  // Construct entire message first to ensure thread-safetyness
  std::stringstream full_msg;
  full_msg << "[BLOCKCHAIN - " << table_name << ", " << ha_thd()->thread_id() << "] " << msg;

  std::cout << full_msg.str() << std::endl;
}

int ha_blockchain::find_current_row(uchar *buf) {
  return find_row(current_position, buf);
}

int ha_blockchain::find_row(my_off_t index, uchar *buf) {

  // set required zero bits
  uint initial_null_bytes = table->s->null_bytes;
  memset(buf, 0, initial_null_bytes);
  uint pos = initial_null_bytes;

  if(in_transaction()) {
    Table_name table_name(table->alias);
    auto& tx = ha_data_get(ha_thd(), table_name)->tx;
    if(index >= tx->table_scan_data.size()) {
      return HA_ERR_END_OF_FILE;
    }

    auto ts_pos = tx->table_scan_data.begin();
    std::advance(ts_pos, index);
    auto* key = &(ts_pos->first);
    auto* value = &(ts_pos->second);
    memcpy(&(buf[pos]), key->data->data(), key->data->size());
    pos += key->data->size();
    memcpy(&(buf[pos]), value->data->data(), value->data->size());
    return 0;
  }

  // Not in transaction --> copy directly from rnd cache
  try {
    Managed_byte_data& mData = rnd_table_scan_data.at(index);
    memcpy(&(buf[pos]), mData.data->data(), mData.data->size());
  } catch (const std::out_of_range&) {
    return HA_ERR_END_OF_FILE;
  }

  return 0;
}

std::unordered_map<Table_name, std::string>* ha_blockchain::parse_eth_contract_config(char *config) {
  auto map = new std::unordered_map<Table_name, std::string>();
  std::string conf(config);
  std::stringstream ss(conf);
  std::string entry;

  while (std::getline(ss, entry, ',')) {
    size_t i = entry.find(':');
    map->insert({entry.substr(0, i).c_str(), entry.substr(i+1, std::string::npos)});
  }

  return map;
}

void ha_blockchain::extract_key(uchar *buf, Byte_data* key) {
  uint initial_null_bytes = table->s->null_bytes;

  // first field of table is key field
  Field* key_field = *(table->field);

  key->data = &(buf[initial_null_bytes]);
  key->data_size = key_field->field_length;
}

void ha_blockchain::extract_value(uchar *buf, ulong key_size, Byte_data* value) {
  uint initial_null_bytes = table->s->null_bytes;

  value->data = &(buf[initial_null_bytes + key_size]);
  value->data_size = table->s->reclength - key_size - initial_null_bytes;
}

// Must be a separate function since has to be called at a different time depending on the operation
// i.e. it can't be called e.g. in the constructor
void ha_blockchain::find_connector(const char* full_table_name) {
  if(connector != nullptr) {
    return;
  }

  std::vector<std::string> nameParts;
  boost::split(nameParts, full_table_name, boost::is_any_of("/"));
  Table_name table_name = nameParts.back();

  switch(config_type) {
    case 0: {
      auto searchAddress = ha_blockchain::table_contract_info->find(table_name);
      std::string contract_address;
      if(searchAddress != ha_blockchain::table_contract_info->end()) {
        contract_address = searchAddress->second;
      }

      connector = std::make_unique<Ethereum>(std::string(config_connection),
                                              contract_address,
                                              std::string(config_eth_from),
                                              config_eth_max_waiting_time);

      break;
    }

    default: std::cout << "Error! Unknown blockchain type" << std::endl;
  }

  // save in THD data
  if(connector != nullptr) {
    auto bc_ha_data = ha_data_get(ha_thd(), table_name);
    bc_ha_data->connector = connector.get();
    log("Stored connector in HA_DATA for " + table_name);
  }
}

void ha_blockchain::init_HAData(THD* thd) {
  auto ha_data = thd->get_ha_data(blockchain_hton->slot);

  // fast exit to avoid mutex locking
  if(ha_data->ha_ptr != nullptr) {
    return;
  }  

  mysql_mutex_lock(&thd->LOCK_thd_data);

  // Check again to ensure is thread-safe
  if(ha_data->ha_ptr == nullptr) {
    ha_data->ha_ptr = new ha_data_map;
  }

  mysql_mutex_unlock(&thd->LOCK_thd_data);
}

bc_ha_data_table_t* ha_blockchain::ha_data_get(THD* thd, Table_name& table) {
  init_HAData(thd);
  auto ha_data = thd->get_ha_data(blockchain_hton->slot);
  auto table_map = static_cast<ha_data_map *>(ha_data->ha_ptr);

  auto table_data = table_map->find(table);
  if(table_data == table_map->end()) {
    auto ha_table_data_ptr = std::make_unique<bc_ha_data_table_t>();
    auto ha_table_data = ha_table_data_ptr.get();
    table_map->insert({table, std::move(ha_table_data_ptr)});
    return ha_table_data;
  }

  return table_data->second.get();
}

ha_data_map* ha_blockchain::ha_data_get_all(THD* thd) {
  init_HAData(thd);
  auto ha_data = thd->get_ha_data(blockchain_hton->slot);
  return static_cast<ha_data_map *>(ha_data->ha_ptr);
}

int ha_blockchain::bc_close_connection(handlerton *hton, THD *thd) {

  std::cout << "[BLOCKCHAIN] Closing connection with THD ID " << thd->thread_id() << std::endl;

  // Free HA Data Map
  auto ha_data = thd->get_ha_data(hton->slot);
  delete static_cast<ha_data_map *>(ha_data->ha_ptr);
  return 0;
}

bool ha_blockchain::in_transaction() {
  return thd_test_options(ha_thd(), (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN ));
}

bool ha_blockchain::use_table_scan_cache() {
  return in_transaction() && config_use_ts_cache;
}

struct st_mysql_storage_engine blockchain_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

static MYSQL_SYSVAR_INT(bc_type, config_type, PLUGIN_VAR_RQCMDARG,
                        "Blockchain type (0 for Ethereum)", nullptr, nullptr, 0,
                        0, 0, 0);

static MYSQL_SYSVAR_STR(bc_connection, config_connection, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Blockchain connection string", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_INT(bc_use_ts_cache, config_use_ts_cache, 0,
                        "Blockchain use table scan cache", nullptr,
                        nullptr, 1,0, 1, 0);

static MYSQL_SYSVAR_INT(bc_tx_prepare_immediately, config_tx_prepare_immediately, 0,
                        "Blockchain transactions: immediately send operations to BC buffer", nullptr,
                        nullptr, 0,0, 1, 0);

static MYSQL_SYSVAR_STR(bc_eth_contracts, config_eth_contracts, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Ethereum store contract addresses", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_STR(bc_eth_tx_contract, config_eth_tx_contract, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Ethereum commit contract address", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_STR(bc_eth_from, config_eth_from, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Ethereum FROM address", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_INT(bc_eth_max_waiting_time, config_eth_max_waiting_time, 0,
                        "Ethereum max. time to wait for transaction mined (in seconds)", nullptr, nullptr, 32,
                        16, 300, 0);

static SYS_VAR *blockchain_system_variables[] = {
    MYSQL_SYSVAR(bc_type), // blockchain type: 0 - ethereum
    MYSQL_SYSVAR(bc_connection), // blockchain connection string (e.g. for Ethereum: http://127.0.0.1:8545)
    MYSQL_SYSVAR(bc_use_ts_cache), // 1 - yes, 0 - no
    MYSQL_SYSVAR(bc_tx_prepare_immediately), // 1 - yes, 0 - no
    MYSQL_SYSVAR(bc_eth_contracts), // Concept: one contract per table, format: tableName1:contractAddress,tableName2:contractAddress,...
    MYSQL_SYSVAR(bc_eth_tx_contract),
    MYSQL_SYSVAR(bc_eth_from),
    MYSQL_SYSVAR(bc_eth_max_waiting_time),
    nullptr
};

mysql_declare_plugin(blockchain){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &blockchain_storage_engine,
    "BLOCKCHAIN",
    "TU Darmstadt DM Group",
    "Blockchain storage engine",
    PLUGIN_LICENSE_GPL,
    blockchain_init_func, /* Plugin Init */
    nullptr,           /* Plugin check uninstall */
    nullptr,           /* Plugin Deinit */
    0x0001 /* 0.1 */,
    0,              /* status variables */
    blockchain_system_variables, /* system variables */
    nullptr,                  /* config options */
    0,                        /* flags */
} mysql_declare_plugin_end;
