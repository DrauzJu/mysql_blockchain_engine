// in node.js
const Web3 = require('web3');
const fs = require('fs');

const CONTRACT_ADDRESS="0xEcD60d97E8320Ce39F63F2D5F50C8569dBB4c03A";
const FROM_ACCOUNT="0x26B5A7711383EB82EC3f72DFBc007491a920D054";

// Connect to Ethereum node
const web3 = new Web3('ws://localhost:8545');

// 1. Load ABI of contract to call
const contractFile = fs.readFileSync("contract_abi/KVStore.json", "utf-8");
const abi = JSON.parse(contractFile).abi;
const kvStore = new web3.eth.Contract(abi, CONTRACT_ADDRESS);

async function put(key, value) {
    const putMethod = kvStore.methods.put(key, value);
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

async function run() {
    // await put(web3.utils.fromDecimal(1), web3.utils.fromDecimal(2));
     await put(web3.utils.fromAscii("Key1"), web3.utils.fromAscii("value1"));
    //await put(web3.utils.fromDecimal(10), web3.utils.fromAscii("Val1"));

    // await tableScan();

    // await remove(web3.utils.fromAscii("Key1"));
     await remove(web3.utils.fromDecimal(10));
    // await remove(web3.utils.fromDecimal(7));

    await tableScan();

    web3.currentProvider.connection.close();
}

run().then(() => console.log("Done"));
