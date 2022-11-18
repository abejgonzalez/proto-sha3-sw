#!/bin/bash

set -ex

echo "Removing old overlay"
rm -rf overlay-all
mkdir -p overlay-all

echo "Copy run script"
cp run.sh overlay-all/run.sh

echo "Building srcs"
MAKE="make -f Makefile-old-proto-repo"
cd src

#echo "Build ss-10-*-all-cpu.riscv"
#$MAKE clean
#$MAKE \
#    proto-sha3-serialized-threads.riscv \
#    proto-sha3-chained.riscv
#
#cp proto-sha3-serialized-threads.riscv ../overlay-all/ss-serial-all-cpu.riscv
#cp proto-sha3-chained.riscv ../overlay-all/ss-chained-all-cpu.riscv

echo "Build ss-*-all-accel.riscv"

$MAKE clean
$MAKE EXTRA_CXXFLAGS="-DSHA3_ACCEL" \
    proto-sha3-serialized-threads.riscv
#    proto-sha3-chained.riscv

cp proto-sha3-serialized-threads.riscv ../overlay-all/ss-serial-all-accel.riscv
#cp proto-sha3-chained.riscv ../overlay-all/ss-chained-all-accel.riscv

echo "Successful builds"
