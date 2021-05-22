#!/bin/bash

mkdir -p pref

git submodule update --init
cd Jinja2Cpp
rm -rf bld
mkdir bld
cd bld

cmake .. -DCMAKE_CXX_FLAGS="-Wno-parentheses" -DCMAKE_INSTALL_PREFIX=../../pref
cmake --build . --target all
cmake --build . --target install

# need to be fixed
# ctest -C Release
cd ..
cd ..

