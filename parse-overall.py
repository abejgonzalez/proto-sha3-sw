#!/usr/bin/env python3

import sys
import re

txtfile = sys.argv[1]
print(f"Parsing {txtfile}...")

cpu_proto_t_sub_i = -1
cpu_sha3_t_sub_i = -1
accel_proto_t_sub_i = -1
accel_sha3_t_sub_i = -1

with open(txtfile) as f:
    for l in f:
        m = re.match(r"^.*CPU Protobuf.*t_sub_i.*= (.*)", l)
        if m:
            cpu_proto_t_sub_i = float(m.group(1))

        m = re.match(r"^.*CPU SHA3.*t_sub_i.*= (.*)", l)
        if m:
            cpu_sha3_t_sub_i = float(m.group(1))

        m = re.match(r"^.*Accel. Protobuf.*t_sub_i.*= (.*)", l)
        if m:
            accel_proto_t_sub_i = float(m.group(1))

        m = re.match(r"^.*Accel. SHA3.*t_sub_i.*= (.*)", l)
        if m:
            accel_sha3_t_sub_i = float(m.group(1))

assert cpu_proto_t_sub_i != -1 and cpu_sha3_t_sub_i != -1 and accel_proto_t_sub_i != -1 and accel_sha3_t_sub_i != -1, f"Something went wrong. Check {txtfile}"

print(f"Avg. Protobuf Ser. s_sub_i = {cpu_proto_t_sub_i/accel_proto_t_sub_i}")
print(f"Avg. SHA3 Hashing  s_sub_i = {cpu_sha3_t_sub_i/accel_sha3_t_sub_i}")
