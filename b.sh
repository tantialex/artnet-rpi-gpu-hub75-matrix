#!/bin/sh

make clean
make -j4
sudo make install
gcc example.c -O3 -mtune=native -lrpihub75_gpu  -o example
