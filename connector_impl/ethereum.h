#ifndef MYSQL_8_0_20_ETHEREUM_H
#define MYSQL_8_0_20_ETHEREUM_H

#include <curl/curl.h>
#include <storage/blockchain/connector.h>
#include <storage/blockchain/blockchain_table_tx.h>
#include <storage/blockchain/types.h>
#include <cassert>
#include <iostream>
#include <regex>
#include <string>
#include <include/my_base.h>
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <iomanip>
#include <thread>
#include <utility>

#include "json.hpp"

#define MINING_CHECK_INTERVAL 200

struct RPC_params {
  std::string from;
  std::string to;
  std::string data;
  std::string method;
  std::string gas;
  std::string gas_price;
  std::string quantity_tag;
  std::string transaction_ID;
  uint64 nonce;
  RPC_params() : nonce(0) {}
};


struct Transaction_confirmation_exception : public std::exception
{
  std::string msg;
  std::string transaction;

  explicit Transaction_confirmation_exception(std::string msg, std::string transaction):
      msg(std::move(msg)), transaction(std::move(transaction)) {}

  const char * what () const noexcept override
  {
    return msg.c_str();
  }
};


struct Transaction_nonce_exception : public std::exception
{
    const char * what () const noexcept override
    {
        return "Transaction nonce is too low / already known";
    }
};


/*
 * ---- HELPER METHODS ----------------------------------
 */

void log(const std::string& msg, const std::string& method = "");
std::string byte_array_to_hex(Byte_data* data, int length=32);

// Implement here since its a template function
template<typename T>
std::string numeric_to_hex(T num, int size=64) {
  std::stringstream ss;
  ss << std::hex;
  ss << std::setw(size) << std::setfill('0') << num;

  return ss.str();
}

// -------------------------
// class definition
// -------------------------

class Ethereum : public table_connector {

  public:
    explicit Ethereum(std::string connection_string,
                   std::string store_contract_address,
                   std::string from_address,
                   int max_waiting_time);
    ~Ethereum() override;

    int get(Byte_data* key, unsigned char* buf, int value_size) override;
    int put(Byte_data* key, Byte_data* value, TXID txID) override;
    int remove(Byte_data *key, TXID txID) override;
    void table_scan_to_vec(std::vector<Managed_byte_data> &tuples, size_t key_length, size_t value_length) override;
    void table_scan_to_map(tx_cache_t& tuples, size_t key_kength, size_t value_length) override;
    int drop_table() override;
    int clear_commit_prepare(boost::uuids::uuid tx_ID) override;

    std::string call(RPC_params params, bool set_gas);
    std::string call(std::string& params, std::string& method);
    std::string check_mining_result(std::string& transaction_ID);

   private:
    std::string _store_contract_address;
    std::string _from_address;
    std::string _connection_string;
    size_t max_waiting_time;
    CURL *curl;
    std::mutex curl_call_mtx;
    static std::mutex nonce_init_mtx;
    static std::atomic_uint64_t nonce;

    std::vector <std::string> table_scan_call();
    static size_t get_table_scan_results_size(std::vector<std::string> response);
};

#endif  // MYSQL_8_0_20_ETHEREUM_H
