pragma solidity ^0.6.8;

contract Transaction  {

    function commit(
        bytes16 txId,
        address[] memory stores)
    public
    {
        for (uint i = 0; i < stores.length; i++) {
            KVStore kv = KVStore(stores[i]);
            kv.commit(txId);
        }

    }

}

interface KVStore {
    function commit(bytes16 txId) external;
}