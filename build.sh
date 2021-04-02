#!/bin/bash

mkdir -p build

pushd lib/tco_libd
./build.sh
mv -f build/tco_libd.a ../../build
popd

pushd lib/tco_linalg
./build.sh
mv -f build/tco_linalg.a ../../build
popd

pushd build
clang \
    -Wall \
    -std=c11 \
    -D _DEFAULT_SOURCE \
    -D TRAINING \
    -I ../code \
    -I ../code/utils \
    -I ../lib/tco_libd/include \
    -I ../lib/tco_linalg/include \
    -I ../lib/tco_shmem \
    -I /usr/lib/aarch64-linux-gnu/glib-2.0/include \
	-I /usr/include/gstreamer-1.0 \
	-I /usr/include/glib-2.0 \
    -l m\
    -l rt \
    -l gstreamer-1.0 \
    -l glib-2.0 \
	-l gobject-2.0 \
    -l pthread \
    `pkg-config --cflags --libs gstreamer-1.0` \
    ../code/*.c \
    ../code/utils/*.c \
    tco_libd.a \
    tco_linalg.a \
    -o tco_pland.bin
popd
