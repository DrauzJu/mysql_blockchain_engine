# Setup

1. Download source code for MySQL 8.0.20
2. Clone this repository in folder `storage/blockchain`


# Useful scripts

## Build (with ninja)

Install Boost library before and set path!

```bash
cd mysql-8.0.20
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -GNinja -DWITH_BOOST=BOOST_PATH_HERE ..
ninja
```

## Initial setup

Only has to be done once

```bash
cd mysql-8.0.20
mkdir test_data_dir
build/bin/mysqld --datadir=$(pwd)/test_data_dir --basedir=$(pwd)/build --initialize-insecure --user=$(whoami)
```

## Run mysqld with blockchain plugin

```bash
cd mysql-8.0.20
mkdir -p build/lib/plugin
cp build/plugin_output_directory/ha_blockchain.so build/lib/plugin

build/bin/mysqld --datadir=$(pwd)/test_data_dir --basedir=$(pwd)/build --plugin-load=ha_blockchain.so \
    --blockchain-bc-type-var=0 --blockchain-bc-connection='http://localhost:8545'
```

## MySQL client

```bash
cd mysql-8.0.20
build/bin/mysql -u root -h localhost -P 3306
```

## Stop mysqld

```bash
sudo killall mysqld
```