#! /bin/bash
cd $(dirname "$0")
SCRIPTDIR=$( pwd )

cmake -B build -DCMAKE_INSTALL_PREFIX=../installation
cmake --build build
cd build
make install
