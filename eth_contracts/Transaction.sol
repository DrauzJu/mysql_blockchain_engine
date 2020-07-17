pragma solidity ^0.6.8;

import "./TableStorage.sol";


contract Transaction  {


    function commit(
        bytes16 txId,
        address[] memory contracts)
    public
    {
        for (uint i = 0; i < contracts.length; i++) {
            KVStore kv = KVStore(contracts[i]);
            kv.commit(txId);
        }

    }

}