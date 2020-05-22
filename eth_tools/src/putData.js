// in node.js
const Web3 = require('web3');
const fs = require('fs');

// Connect to Ethereum node
const web3 = new Web3('ws://localhost:8545');

// 1. Load ABI of contract to call
const contractFile = fs.readFileSync("contract_abi/KVStore.json", "utf-8");
const abi = JSON.parse(contractFile).abi;

// 2. Get "put" method object
const kvStore = new web3.eth.Contract(abi, "0x8285fa89Cd178361076CecBd4314B6e945a76F8B");
const putMethod = kvStore.methods.put(web3.utils.fromAscii("key1"), web3.utils.fromAscii("value1"));

// 3. Call "put" method
putMethod.send({
    from: "0xc1c0dCAaF9b9F08fb049561a7DD75bb56121bb9a",   // test account created by Ganache
    gas: Math.pow(10, 6)                            // max. gas to be used
}).then(async () => {
    await tableScan();
    await remove();
    await tableScan();
    web3.currentProvider.connection.close();
});

async function tableScan() {
    const tsMethod = kvStore.methods.tableScan();
    const receipt = await tsMethod.call({
        from: web3.eth.defaultAccount
    });
    console.log(receipt);
}

async function remove() {
    const removeMethod = kvStore.methods.remove(web3.utils.fromAscii("key1"));
    await removeMethod.send({
        from: "0xc1c0dCAaF9b9F08fb049561a7DD75bb56121bb9a",   // test account created by Ganache
        gas: Math.pow(10, 6)                            // max. gas to be used
    });
}

