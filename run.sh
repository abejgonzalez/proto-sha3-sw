#!/bin/bash

set -e

TO_RUN=$1

cat /proc/cpuinfo

# note: this indexing only works on rv-linux on fsim (not qemu)

# ordered
PLINES=$(grep "processor" /proc/cpuinfo)
HLINES=$(grep "hart" /proc/cpuinfo)

P0_CID=$(echo "$PLINES" | head -n1)
P0_CID=${P0_CID:0-1}
P0_HID=$(echo "$HLINES" | head -n1)
P0_HID=${P0_HID:0-1}
echo "$P0_CID $P0_HID"

P1_CID=$(echo "$PLINES" | head -n2 | tail -n1)
P1_CID=${P1_CID:0-1}
P1_HID=$(echo "$HLINES" | head -n2 | tail -n1)
P1_HID=${P1_HID:0-1}
echo "$P1_CID $P1_HID"

P2_CID=$(echo "$PLINES" | head -n3 | tail -n1)
P2_CID=${P2_CID:0-1}
P2_HID=$(echo "$HLINES" | head -n3 | tail -n1)
P2_HID=${P2_HID:0-1}
echo "$P2_CID $P2_HID"

# proto is on hartid 0
if [ $P0_HID -eq "0" ]; then
    PROTO_CID=$P0_CID
fi
if [ $P1_HID -eq "0" ]; then
    PROTO_CID=$P1_CID
fi
if [ $P2_HID -eq "0" ]; then
    PROTO_CID=$P2_CID
fi

# sha3 is on hartid 1
if [ $P0_HID -eq "1" ]; then
    SHA3_CID=$P0_CID
fi
if [ $P1_HID -eq "1" ]; then
    SHA3_CID=$P1_CID
fi
if [ $P2_HID -eq "1" ]; then
    SHA3_CID=$P2_CID
fi

echo "Running $TO_RUN with {Proto, SHA3} CID's: {$PROTO_CID, $SHA3_CID}"
$TO_RUN $PROTO_CID $SHA3_CID
