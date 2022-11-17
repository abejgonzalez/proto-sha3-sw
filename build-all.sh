#!/bin/bash

set -ex

# core ids
PROTO_CID=1
SHA3_CID=2

# getopts does not support long options, and is inflexible
while [ "$1" != "" ];
do
    case $1 in
        -proto-cid)
            shift
            PROTO_CID=$1 ;;
        -sha3-cid)
            shift
            SHA3_CID=$1 ;;
        * )
            error "invalid option $1"
            exit 1 ;;
    esac
    shift
done

EXTRA_CXXFLAGS="-DPROTO_CID=${PROTO_CID} -DSHA3_CID=${SHA3_CID}"

echo "Removing old overlay"
rm -rf overlay-all
mkdir -p overlay-all

echo "Building srcs"
MAKE="make -f Makefile-old-proto-repo"
cd src

echo "Build ss-10-*-all-cpu.riscv"
$MAKE clean
$MAKE EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS}" \
    proto-sha3-serialized-threads.riscv \
    proto-sha3-chained.riscv

cp proto-sha3-serialized-threads.riscv ../overlay-all/ss-10-serial-all-cpu.riscv
cp proto-sha3-chained.riscv ../overlay-all/ss-10-chained-all-cpu.riscv

echo "Build ss-10-*-all-accel.riscv"
$MAKE clean
$MAKE EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS} -DPROTO_ACCEL -DSHA3_ACCEL" \
    proto-sha3-serialized-threads.riscv \
    proto-sha3-chained.riscv

cp proto-sha3-serialized-threads.riscv ../overlay-all/ss-10-serial-all-accel.riscv
cp proto-sha3-chained.riscv ../overlay-all/ss-10-chained-all-accel.riscv

echo "Successful builds"
