#!/bin/bash
# sudo apt-get install mingw-w64 g++-mingw-w64

cmake -DCMAKE_TOOLCHAIN_FILE=Toolchain-Ubuntu-mingw64.cmake ./
make
rm -rf CMakeFiles
rm CMakeCache.txt
rm cmake_install.cmake
rm Makefile


