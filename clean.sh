#!/bin/bash

pushd lib/tco_libd
./clean.sh
popd

pushd lib/tco_linalg
./clean.sh
popd

rm -r ./build/*
