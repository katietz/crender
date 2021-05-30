#!/bin/bash

mkdir -p bin

# copy default conda build configuration file next to binary
cp data/conda_build_config.yaml bin/.

cd src
g++ --std=gnu++14 -o ../bin/crender main.cc prepro.cc yaml.cc lint.cc version.cc -L ../pref/lib/static/ -L../pref/lib -I ../pref/include/ -ljinja2cpp -lpthread -I ../yaml-cpp/include/ -lyaml-cpp
cd ..

