// in node.js
const Web3 = require('web3');
const fs = require('fs');

const CONTRACT_ADDRESS="0xa23BF85B8b197F848590c88A9a7E76b649231F0D";
const FROM_ACCOUNT="0x15937E0c7Ea7Eb75f83982e202f4EAd585E382c5";

// Connect to Ethereum node
const web3 = new Web3('ws://localhost:8545');

// 1. Load ABI of contract to call
const contractFile = fs.readFileSync("contract_abi/KVStore.json", "utf-8");
const abi = JSON.parse(contractFile).abi;
const kvStore = new web3.eth.Contract(abi, CONTRACT_ADDRESS);

const contractTxFile = fs.readFileSync("contract_abi/Transaction.json", "utf-8");
const abiTx = JSON.parse(contractTxFile).abi;
const tx = new web3.eth.Contract(abiTx, "0xbE53c062F05f9BB3582B9866E4d70e2DaF74D4c4");

async function put(key, value, txId=null) {
    const putMethod = (txId === null) ? kvStore.methods.put(key, value) : kvStore.methods.put(key, value, txId);
    // const estimateGas = await putMethod.estimateGas({gas: 1});
    // console.log("Estimating " + estimateGas + " for put");
    console.log("Estimated gas for put: " + await putMethod.estimateGas());

    await putMethod.send({
        from: FROM_ACCOUNT,
        gas: 200000
    });
}

async function putBatch(keys, values) {
    const putBatchMethod = kvStore.methods.putBatch(keys, values);
    // const estimateGas = await putMethod.estimateGas({gas: 1});
    // console.log("Estimating " + estimateGas + " for put");
    console.log("Estimated gas for putBatch: " + await putBatchMethod.estimateGas());

    await putBatchMethod.send({
        from: FROM_ACCOUNT,
        gas: 200000
    });
}

async function tableScan() {
    const tsMethod = kvStore.methods.tableScan();
    const receipt = await tsMethod.call({
        from: FROM_ACCOUNT
    });

    console.log(receipt);
}

async function remove(key) {
    const removeMethod = kvStore.methods.remove(key);
    await removeMethod.send({
        from: FROM_ACCOUNT,
        gas: 100000
    });
}

async function get(key) {
    const getMethod = kvStore.methods.get(key);
    const receipt = await getMethod.call({
        from: FROM_ACCOUNT,
        gas: 100000
    });

    console.log(receipt);
}

async function commit(txId) {
    const commitMethod = tx.methods.commit(txId,[CONTRACT_ADDRESS]);
    await commitMethod.send({
        from: FROM_ACCOUNT,
        gas: 100000
    });
}

async function run() {
     await put(web3.utils.fromDecimal(1), web3.utils.fromDecimal(2));
     await put(web3.utils.fromAscii("txKey0"), web3.utils.fromAscii("value1"), web3.utils.fromAscii("txid2"));
     await put(web3.utils.fromAscii("txKey1"), web3.utils.fromAscii("value2"), web3.utils.fromAscii("txid2"));
    //await put(web3.utils.fromDecimal(10), web3.utils.fromAscii("Val1"));

    await commit(web3.utils.fromAscii("txid2"));


    // await tableScan();

    // await remove(web3.utils.fromAscii("Key2"));
    // await remove(web3.utils.fromDecimal(1));
    // await remove(web3.utils.fromDecimal(7));

    await tableScan();

    web3.currentProvider.connection.close();
}

run().then(() => console.log("Done"));
