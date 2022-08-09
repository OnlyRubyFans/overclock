#!/bin/bash

mkdir -p ~/esp
pushd ~/esp
git clone -b v4.4.1 --recursive https://github.com/espressif/esp-idf.git
pushd ~/esp/esp-idf
./install.sh esp32
. $HOME/esp/esp-idf/export.sh

export IDF_PATH=~/esp/esp-idf

popd
popd
cd esp32-src/clock
make
