#!/bin/bash

set -e

echo "Building srcs"
cd src
make
cd ..

echo "Removing old overlay"
rm -rf overlay

echo "Moving built items to overlay"
mkdir -p overlay
mv src/*.riscv overlay/
