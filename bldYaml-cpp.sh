#!/bin/bash

mkdir -p pref

git submodule update --init
cd yaml-cpp
rm -rf bld
mkdir bld
cd bld

cmake .. -DCMAKE_CXX_FLAGS="-Wno-parentheses" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../../pref
cmake --build . --target all
cmake --build . --target install

cd ..
cd ..

