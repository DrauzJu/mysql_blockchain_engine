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
#include <sql/table.h>
#include <iostream>
#include <vector>

#include "my_dbug.h"
#include "mysql/plugin.h"
#include "sql/sql_class.h"
#include "sql/sql_plugin.h"
#include "typelib.h"
#include "mysql/components/services/log_builtins.h"
#include "sql/field.h"

#include "connector_impl/ethereum.h"

static handler *blockchain_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool partitioned, MEM_ROOT *mem_root);

handlerton *blockchain_hton;

// System variables for configuration
static int config_type;
static char* config_connection;
static char* config_eth_contracts;

/* Interface to mysqld, to check system tables supported by SE */
static bool blockchain_is_supported_system_table(const char *db,
                                              const char *table_name,
                                              bool is_sql_layer_system_table);

Blockchain_share::Blockchain_share() { thr_lock_init(&lock); }

static int blockchain_init_func(void *p) {
  DBUG_TRACE;

  blockchain_hton = (handlerton *)p;
  blockchain_hton->state = SHOW_OPTION_YES;
  blockchain_hton->create = blockchain_create_handler;
  blockchain_hton->flags = HTON_CAN_RECREATE;
  blockchain_hton->is_supported_system_table = blockchain_is_supported_system_table;

  // Parse configuration
  if(config_type == ETHEREUM) {
    ha_blockchain::tableContractInfo =
        ha_blockchain::parseEthContractConfig(config_eth_contracts);
  }

  return 0;
}

/**
  @brief
  Example of simple lock controls. The "share" it creates is a
  structure we will pass to each blockchain handler. Do you have to have
  one of these? Well, you have pieces that are used for locking, and
  they are needed to function.
*/

Blockchain_share *ha_blockchain::get_share() {
  Blockchain_share *tmp_share;

  DBUG_TRACE;

  lock_shared_ha_data();
  if (!(tmp_share = static_cast<Blockchain_share *>(get_ha_share_ptr()))) {
    tmp_share = new Blockchain_share;
    if (!tmp_share) goto err;

    set_ha_share_ptr(static_cast<Handler_share *>(tmp_share));
  }
err:
  unlock_shared_ha_data();
  return tmp_share;
}

static handler *blockchain_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       bool, MEM_ROOT *mem_root) {
  return new (mem_root) ha_blockchain(hton, table);
}

std::unordered_map<std::string, std::string>* ha_blockchain::tableContractInfo =
    new std::unordered_map<std::string, std::string>();

ha_blockchain::ha_blockchain(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {}

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

int ha_blockchain::open(const char *, int, uint, const dd::Table *) {
  DBUG_TRACE;

  std::stringstream msg;
  msg << "Opening table";
  log(msg.str());

  switch(config_type) {
    case 0: {
      auto searchAddress = ha_blockchain::tableContractInfo->find(std::string(table->alias));
      if(searchAddress == ha_blockchain::tableContractInfo->end()) {
        connector = new Ethereum("");
      } else {
        connector = new Ethereum(searchAddress->first);
      }

      break;
    }

    default: std::cout << "Error! Unknown blockchain type" << std::endl;
  }

  if (!(share = get_share())) return 1;
  thr_lock_data_init(&share->lock, &lock, nullptr);

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

int ha_blockchain::close(void) {
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
  uint initial_null_bytes = table->s->null_bytes;

  // 1. step: extract key --> first field
  Field* key_field = *(table->field);
  uint key_size = key_field->pack_length();
  ByteData* key = new ByteData(&(buf[initial_null_bytes]), key_size);

  // 2. step: extract values --> other fields
  ulong value_size = table->s->reclength - key_size - initial_null_bytes;
  ByteData* value = new ByteData(&(buf[initial_null_bytes + key_size]),
                                 value_size);

  return connector->put(table->alias, key, value);
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
int ha_blockchain::update_row(const uchar *, uchar *) {
  DBUG_TRACE;
  return HA_ERR_WRONG_COMMAND;
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

int ha_blockchain::delete_row(const uchar *) {
  DBUG_TRACE;
  return HA_ERR_WRONG_COMMAND;
}

/**
  @brief
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.
*/

int ha_blockchain::index_read_map(uchar *, const uchar *, key_part_map,
                               enum ha_rkey_function) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
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
  log("Preparing for table scan");

  // Getting all keys and initializing position
  // current_position = 0;
  // keys = connector->getAllKeys(table->alias);

  current_position = -1;
  if(tableScanData == nullptr) {
    tableScanData = connector->tableScan(table->alias);
  }

  DBUG_TRACE;
  return 0;
}

int ha_blockchain::rnd_end() {
  DBUG_TRACE;
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

  int position = my_get_ptr(pos, ref_length);
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
int ha_blockchain::external_lock(THD *, int) {
  DBUG_TRACE;
  return 0;
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
                                       enum thr_lock_type lock_type) {
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) lock.type = lock_type;
  *to++ = &lock;

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
int ha_blockchain::delete_table(const char *, const dd::Table *) {
  DBUG_TRACE;
  /* This is not implemented but we want someone to be able that it works. */
  return 0;
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

static MYSQL_THDVAR_STR(last_create_thdvar, PLUGIN_VAR_MEMALLOC, nullptr,
                        nullptr, nullptr, nullptr);

static MYSQL_THDVAR_UINT(create_count_thdvar, 0, nullptr, nullptr, nullptr, 0,
                         0, 1000, 0);

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
  /*
    This is not implemented but we want someone to be able to see that it
    works.
  */

  std::stringstream logMsg;
  logMsg << "Creating new table: " << name << std::endl;
  log(logMsg.str());

  // LogErr(SYSTEM_LEVEL, ER_LOG_PRINTF_MSG, logMsg.str());

  /*
    It's just an example of THDVAR_SET() usage below.
  */
  THD *thd = ha_thd();
  char *buf = (char *)my_malloc(PSI_NOT_INSTRUMENTED, SHOW_VAR_FUNC_BUFF_SIZE,
                                MYF(MY_FAE));
  snprintf(buf, SHOW_VAR_FUNC_BUFF_SIZE, "Last creation '%s'", name);
  THDVAR_SET(thd, last_create_thdvar, buf);
  my_free(buf);

  uint count = THDVAR(thd, create_count_thdvar) + 1;
  THDVAR_SET(thd, create_count_thdvar, &count);

  return 0;
}

void ha_blockchain::log(std::string msg) {
  std::cout << "[BLOCKCHAIN - " << table->alias << "] " << msg << std::endl;
}

int ha_blockchain::find_current_row(uchar *buf) {
  return find_row(current_position, buf);
}

int ha_blockchain::find_row(int index, uchar *buf) {
  /*ByteData* key = &(keys[current_position]);

  if(key == nullptr) {
    return HA_ERR_END_OF_FILE;
  }

  uint initial_null_bytes = table->s->null_bytes;
  memset(buf, 0, initial_null_bytes);
  int success = connector->get(table->alias, key, buf, initial_null_bytes);*/

  // --------------------------------------------

  ByteData* data = &(tableScanData[index]);

  if(data == nullptr) {
    return HA_ERR_END_OF_FILE;
  }

  // set required zero bits
  uint initial_null_bytes = table->s->null_bytes;
  memset(buf, 0, initial_null_bytes);
  uint pos = initial_null_bytes;

  // Copy data
  memcpy(&(buf[pos]), data->data, data->dataSize);
  return 0;
}

std::unordered_map<std::string, std::string>* ha_blockchain::parseEthContractConfig(char *config) {
  auto map = new std::unordered_map<std::string, std::string>();
  std::string conf(config);
  std::stringstream ss(conf);
  std::string entry;

  while (std::getline(ss, entry, ',')) {
    size_t i = entry.find(':');
    map->insert({entry.substr(0, i).c_str(), entry.substr(i+1, std::string::npos)});
  }

  return map;
}

struct st_mysql_storage_engine blockchain_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

static MYSQL_SYSVAR_INT(bc_type, config_type, PLUGIN_VAR_RQCMDARG,
                        "Blockchain type (0 for Ethereum)", nullptr, nullptr, 0,
                        0, 0, 0);

static MYSQL_SYSVAR_STR(bc_connection, config_connection, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Blockchain connection string", nullptr, nullptr,
                        nullptr);

static MYSQL_SYSVAR_STR(bc_eth_contracts, config_eth_contracts, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
                        "Ethereum contract addresses", nullptr, nullptr,
                        nullptr);

static SYS_VAR *blockchain_system_variables[] = {
    MYSQL_SYSVAR(bc_type), // blockchain type: 0 - ethereum
    MYSQL_SYSVAR(bc_connection), // blockchain connection string (e.g. for Ethereum: http://127.0.0.1:8545)
    MYSQL_SYSVAR(bc_eth_contracts), // Concept: one contract per table, format: tableName1:contractAddress,tableName2:contractAddress,...
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
