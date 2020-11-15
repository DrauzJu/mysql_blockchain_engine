pragma solidity ^0.7.4;
pragma experimental ABIEncoderV2;
// SPDX-License-Identifier: UNLICENSED

contract Transaction  {

    struct TxOperation {
        bytes32 key;
        bytes32 value;
        bool deleteEntry;
        address contractStore;
    }
    
    struct TxOperationPerStore {
        bool containsData;
        bytes32[] keys;
        bytes32[] values;
        bytes32[] removeKeys;
    }
    
    mapping(address => TxOperationPerStore) private opsPerStore; // can only live in storage
    address[] private stores; // to iterate over mapping, as state variable in storage to have push()
    
    // Used when prepareImmediately is enabled: 
    // commit all single operations in all tables, which were sent to the blockchain already before
    function commitAll(
        bytes16 txId,
        address[] memory storesToCommit)
    public
    {
        for (uint i = 0; i < storesToCommit.length; i++) {
            KVStore kv = KVStore(storesToCommit[i]);
            kv.commit(txId);
        }
    }

    function commitAll(
        TxOperation[] memory operations)
    public
    {
        
        // Get all operations per table
        for (uint i = 0; i < operations.length; i++) {
            address store = operations[i].contractStore;
            bytes32 key = operations[i].key;
            bytes32 value = operations[i].value;
            bool deleteEntry = operations[i].deleteEntry;
            
            // Add to list of stores if not done yet
            if(!opsPerStore[store].containsData) {
                stores.push(store);
                opsPerStore[store].containsData = true;
            }
            
            if(deleteEntry) {
                opsPerStore[store].removeKeys.push(key);
            } else {
                opsPerStore[store].keys.push(key);
                opsPerStore[store].values.push(value);
            }
        }
        
        // Call writeBatch for each store
        for (uint i = 0; i < stores.length; i++) {
            address store = stores[i];
            
            KVStore kv = KVStore(store);
            kv.writeBatch(opsPerStore[store].keys, opsPerStore[store].values, opsPerStore[store].removeKeys);
        }
        
        // Clear stores
        delete stores;
    }
}

interface KVStore {
    function writeBatch(bytes32[] memory keys, bytes32[] memory values, bytes32[] memory removeKeys) external;
    function commit(bytes16 txId) external;
}
