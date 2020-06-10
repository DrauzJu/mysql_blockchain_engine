const Web3 = require('web3');
const fs = require('fs');

const FROM_ACCOUNT="0xab81353220deccf2e8f931533ebaebd9348dd615";
const COMPILED_CONTRACT = "contract_abi/KVStore.json";


// Connect to Ethereum node
const web3 = new Web3('http://localhost:8000');

const contractFile = JSON.parse(fs.readFileSync(COMPILED_CONTRACT, "utf-8"));
const contract = new web3.eth.Contract(contractFile.abi);

contract.deploy({data: contractFile.bytecode}).send({from: FROM_ACCOUNT}).then((newContract) => {
    const address = newContract.options.address;
    if(address) {
        console.log("Deployed successfully: " + address);
    } else {
        console.error("Deployment failed!");
    }
});
