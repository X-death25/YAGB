#!/bin/bash

cmake ./
make
rm -rf CMakeFiles
rm CMakeCache.txt
rm cmake_install.cmake
rm Makefile


