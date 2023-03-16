#!/usr/bin/env python3

import sys
import re

uartlog = sys.argv[1]
print(f"Parsing {uartlog}...")

def avg(l):
    l = list(l)
    return sum(l)/len(l)

dup_iteration_lines = {}
with open(uartlog) as f:
    cur_itr = -1
    for l in f:
        m = re.match(r"^Dup Itr: (.*)", l)
        if m:
            itr = int(m.group(1))
            dup_iteration_lines[itr] = [l]
            cur_itr = itr
        else:
            if cur_itr == -1:
                continue
            else:
                old_lines = dup_iteration_lines[cur_itr]
                dup_iteration_lines[cur_itr] = old_lines + [l]

dup_iterations = {}

overall_dup_cycles = {}
k = sorted(list(dup_iteration_lines.keys()))[-1]
for l in dup_iteration_lines[k]:
    m = re.match(r"^Overall\[(.*)\]: (.*)", l)
    if m:
        overall_dup_cycles[int(m.group(1))] = int(m.group(2))
        continue

for k in dup_iteration_lines:
    lines = dup_iteration_lines[k]

    sha3_core_cycles = {}
    sha3_wait_cycles = {}
    sha3_loop_cycles = -1
    sha3_overall_thread_cycles = -1

    proto_obj_setup = -1
    proto_accel_setup = -1
    proto_itr_cycles = {}
    proto_thread_overall = -1

    for l in lines:
        m = re.match(r"^SHA3: Iter (.*): SHACore\=(.*) SHAWait\=(.*)", l)
        if m:
            sha3_core_cycles[int(m.group(1))] = int(m.group(2))
            sha3_wait_cycles[int(m.group(1))] = int(m.group(3))
            continue

        m = re.match(r"^SHA3: LoopCycles: (.*)", l)
        if m:
            sha3_loop_cycles = int(m.group(1))
            continue

        m = re.match(r"^SHA3: OverallThreadCycles: (.*)", l)
        if m:
            sha3_overall_thread_cycles = int(m.group(1))
            continue

        m = re.match(r"^PRO: AccelSetup=(.*)", l)
        if m:
            proto_accel_setup = int(m.group(1))
            continue

        m = re.match(r"^PRO: ProtoObjSetup=(.*)", l)
        if m:
            proto_obj_setup = int(m.group(1))
            continue

        m = re.match(r"^PRO: Iter (.*): IterSerTime\=(.*)", l)
        if m:
            itr = int(m.group(1))
            proto_itr_cycles[itr] = int(m.group(2))
            continue

        m = re.match(r"^PRO: ProtoThreadOverall\=(.*)", l)
        if m:
            proto_thread_overall = int(m.group(1))
            continue

    assert bool(sha3_core_cycles)
    assert bool(sha3_wait_cycles)
    assert not sha3_loop_cycles == -1
    assert not sha3_overall_thread_cycles == -1

    assert not proto_accel_setup == -1
    assert not proto_obj_setup == -1
    assert bool(proto_itr_cycles)
    assert not proto_thread_overall == -1

    overall_dup_c = overall_dup_cycles[k]

    dup_iterations[k] = [
        overall_dup_c,
    ]

overall_dup_c_i = []

for k, e in dup_iterations.items():
    overall_dup_c_i += [e[0]]

print(f"Avg. t'_e2e = {avg(overall_dup_c_i)}")
print(f"Min. t'_e2e = {min(overall_dup_c_i)}")
print(f"Max. t'_e2e = {max(overall_dup_c_i)}")
