// in node.js
const Web3 = require('web3');
const fs = require('fs');

// Connect to Ethereum node
const web3 = new Web3('ws://localhost:8545');

// 1. Load ABI of contract to call
const contractFile = fs.readFileSync("contract_abi/KVStore.json", "utf-8");
const abi = JSON.parse(contractFile).abi;

// 2. Get "put" method object
const kvStore = new web3.eth.Contract(abi, "0x844698BAE12D253C9682C2FBAf1C1cD8A7607565");
const putMethod = kvStore.methods.put(web3.utils.fromAscii("key1"), web3.utils.fromAscii("value1"));

// 3. Call "put" method
putMethod.send({
    from: "0xc1c0dCAaF9b9F08fb049561a7DD75bb56121bb9a",   // test account created by Ganache
    gas: Math.pow(10, 6)                            // max. gas to be used
}).then(() => {
    tableScan();
});

function tableScan() {
    const tsMethod = kvStore.methods.tableScan();
    tsMethod.call({
        from: web3.eth.defaultAccount
    }).then((receipt) => {
        console.log(receipt);
        web3.currentProvider.connection.close();
    });
}

