#!/bin/bash

cd Jinja2Cpp
mkdir -p bld
cd bld

cmake --build . --target install

cd ..
cd ..

