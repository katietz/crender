#!/bin/bash

mkdir -p bin

cd src
g++ -o ../bin/crender main.cc prepro.cc yaml.cc lint.cc -L ../pref/lib/static/ -L../pref/lib -I ../pref/include/ -ljinja2cpp -lpthread -I ../yaml-cpp/include/ -lyaml-cpp
cd ..

