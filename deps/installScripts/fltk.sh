#! /bin/bash
cd $(dirname "$0")

#XXX is it possible to skip compiling the example programs?

#NOTE removing the build directory works if changing settings

cd ../sources/fltk
cmake -B build -DCMAKE_INSTALL_PREFIX=../../installation
cmake --build build
cd build
make install
