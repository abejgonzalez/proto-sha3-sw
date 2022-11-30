#!/bin/bash

set -ex

OVERLAY=overlay-all-hyper

echo "Removing old overlay"
rm -rf $OVERLAY
mkdir -p $OVERLAY

echo "Copy run script"
cp run.sh $OVERLAY/run.sh

echo "Building srcs"
MAKE="make"
cd new-src

EXTRA_CXXFLAGS="-DDEBUG"

echo "Build ss-*-all-accel.riscv"

$MAKE clean
$MAKE EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS} -DPROTO_ACCEL -DSHA3_ACCEL" \
    proto-sha3-chained.riscv

cp proto-sha3-chained.riscv ../$OVERLAY/ss-chained-all-accel.riscv

echo "Successful builds"
