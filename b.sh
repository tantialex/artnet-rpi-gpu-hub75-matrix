#!/bin/sh

make clean
make -j4
sudo make install
gcc main.c -O3 -mtune=native -lrpihub75_gpu  -o example
