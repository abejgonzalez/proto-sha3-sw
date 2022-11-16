#!/bin/bash

set -e

USE_PROTO_ACCEL=false
USE_SHA3_ACCEL=false
# core ids
PROTO_CID=1
SHA3_CID=2

# getopts does not support long options, and is inflexible
while [ "$1" != "" ];
do
    case $1 in
        -acc-proto)
            USE_PROTO_ACCEL=true ;;
        -acc-sha3)
            USE_SHA3_ACCEL=true ;;
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

if [ "$USE_PROTO_ACCEL" = true ]; then
	EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS} -DPROTO_ACCEL"
fi

if [ "$USE_SHA3_ACCEL" = true ]; then
	EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS} -DSHA3_ACCEL"
fi

echo "Building srcs"
cd src
make EXTRA_CXXFLAGS="${EXTRA_CXXFLAGS}"
cd ..

echo "Removing old overlay"
rm -rf overlay

echo "Moving built items to overlay"
mkdir -p overlay
mv src/*.riscv overlay/
