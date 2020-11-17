#ifndef MYSQL_8_0_20_TRANSACTION_ETHEREUM_H
#define MYSQL_8_0_20_TRANSACTION_ETHEREUM_H

#include <string>
#include <storage/blockchain/connector.h>

class transaction_ethereum : public transaction_connector {

  public:
    int write_batch(std::vector<Put_op> * put_data, std::vector<Remove_op> * remove_data) override;
    int atomic_commit(TXID tx_ID, const std::vector<Table_name>& affected_tables) override;

    static transaction_ethereum* get_instance() {
      if(instance == nullptr) {
        instance = new transaction_ethereum;
      }

      return instance;
    }

    static void set_parameters(std::string connection_string,
                        std::string from_address,
                        int max_waiting_time,
                        std::string commit_contract_address,
                        std::unordered_map<Table_name, std::string>* table_contract_info) {
      _connection_string = std::move(connection_string);
      _from_address = std::move(from_address);
      _max_waiting_time = max_waiting_time;
      _commit_contract_address = std::move(commit_contract_address);
      _table_contract_info = table_contract_info;                   
    }

  private:
    static std::string _connection_string;
    static std::string _from_address;
    static int _max_waiting_time;
    static std::string _commit_contract_address;
    static std::unordered_map<Table_name, std::string>* _table_contract_info;
    static transaction_ethereum* instance;

    explicit transaction_ethereum();
    void translate_table_names(const std::vector<Table_name>& tables, std::vector<std::string>* addresses);
};

#endif // MYSQL_8_0_20_TRANSACTION_ETHEREUM_H
