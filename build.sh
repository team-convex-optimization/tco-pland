#!/bin/bash

mkdir -p build

pushd lib/tco_libd
./build.sh
mv -f build/tco_libd.a ../../build
popd

pushd build
clang \
    -Wall \
    -std=c11 \
    -I ../code \
    -I ../lib/tco_libd/include \
    -I ../lib/tco_shmem \
    ../code/*.c \
    tco_libd.a \
    -o tco_pland.bin
popd
