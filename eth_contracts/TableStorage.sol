pragma solidity >=0.4.21 <0.7.0;

/// @author melhindi
/// @title A key-value storage contract with a batch interface
contract KVStore {

    // tightly packed struct
    struct Value
    {
        uint blocknumber; // indicates when value was written
        bytes32 value;
    }

    mapping(bytes32 => Value) private data; // data store
    bytes32[] internal keyList;              // list of keys of data

    /// Store the pair key:value in the storage.
    /// @param key the new key to store
    /// @param value the value corresponding to the key
    /// @dev stores the number in the state variable `data`
    function put(
        bytes32 key,
        bytes32 value)
    public
    {
        Value memory v = Value(block.number,value);
        // persist data in blockchain
        data[key] = v;
        keyList.push(key);
    }

    /// Stores multiplie key:value pairs in the storage.
    /// @param keys An array of keys to store
    /// @param values An array of values correpsonding to the keys,
    ///  thereby keys[i] should correspond to values[i]
    /// @dev stores the number in the state variable `data`
    function putBatch(
        bytes32[] memory keys,
        bytes32[] memory values)
    public
    {
        for (uint i = 0; i < keys.length; i++) {
            Value memory v = Value(block.number,values[i]);
            data[keys[i]] = v;
            keyList.push(keys[i]);
        }
    }

    function tableScan()
    public
    view
    returns (bytes32[] memory keys, bytes32[] memory values)
    {
        uint size = keyList.length;
        keys = new bytes32[](size);
        values = new bytes32[](size);

        for(uint i=0; i<size; i++) {
            keys[i] = keyList[i];
            values[i] = data[keyList[i]].value;
        }

        return (keys, values);
    }

    function get(
        bytes32 key)
    public
    view
    returns (bytes32 value, uint blocknumber)
    {
        Value memory v = data[key];
        value = v.value;
        blocknumber = v.blocknumber;

        return (value, blocknumber);
    }

    function getBatch(
        bytes32[] memory keys)
    public
    view
    returns (bytes32[] memory values, uint[] memory blocknumbers)
    {
        uint length = keys.length;
        values = new bytes32[](length);
        blocknumbers = new uint[](length);
        for (uint i = 0; i < length; i++) {
            Value memory v = data[keys[i]];
            values[i] = v.value;
            blocknumbers[i] = v.blocknumber;
        }
        return (values, blocknumbers);
    }

}
