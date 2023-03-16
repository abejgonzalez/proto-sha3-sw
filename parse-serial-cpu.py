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

    sha3_full_cycles = {}
    sha3_core_cycles = {}
    sha3_setup_cycles = {}
    sha3_start_pretlb = -1
    sha3_pretlb_start = -1
    sha3_loop_cycles = -1
    sha3_overall_thread_cycles = -1
    proto_accel_setup = -1
    proto_obj_setup = -1
    proto_itr_cycles = {}
    proto_blocked_time = -1
    proto_thread_overall = -1

    for l in lines:
        m = re.match(r"^SHA3: Iter (.*): SHAFull\=(.*) SHACore\=(.*) SHASetup\=(.*)", l)
        if m:
            sha3_full_cycles[int(m.group(1))] = int(m.group(2))
            sha3_core_cycles[int(m.group(1))] = int(m.group(3))
            sha3_setup_cycles[int(m.group(1))] = int(m.group(4))
            continue

        m = re.match(r"^SHA3: Start->PreTLB: (.*)", l)
        if m:
            sha3_start_pretlb = int(m.group(1))
            continue

        m = re.match(r"^SHA3: PreTLB->Start: (.*)", l)
        if m:
            sha3_pretlb_start = int(m.group(1))
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

        m = re.match(r"^PRO: BlockedTime\=(.*)", l)
        if m:
            proto_blocked_time = int(m.group(1))
            continue

        m = re.match(r"^PRO: ProtoThreadOverall\=(.*)", l)
        if m:
            proto_thread_overall = int(m.group(1))
            continue

    assert bool(sha3_full_cycles)
    assert bool(sha3_core_cycles)
    assert bool(sha3_setup_cycles)
    assert not sha3_start_pretlb == -1
    assert not sha3_pretlb_start == -1
    assert not sha3_loop_cycles == -1
    assert not sha3_overall_thread_cycles == -1

    assert not proto_accel_setup == -1
    assert not proto_obj_setup == -1
    assert bool(proto_itr_cycles)
    assert not proto_blocked_time == -1
    assert not proto_thread_overall == -1

    cpu_proto_t_sub_i = sum(proto_itr_cycles.values())
    cpu_sha3_t_sub_i = sum(sha3_full_cycles.values())
    #print(f"{overall_dup_cycles[k]} - {cpu_proto_t_sub_i} - {cpu_sha3_t_sub_i}")
    cpu_other_non_accel_t_sub_i = overall_dup_cycles[k] - cpu_proto_t_sub_i - cpu_sha3_t_sub_i

    acc_proto_t_sub_i = sum(proto_itr_cycles.values()) + proto_blocked_time
    acc_proto_t_setup_i = proto_accel_setup

    acc_sha3_t_sub_i = sum(sha3_core_cycles.values())
    acc_sha3_t_setup_i = sha3_pretlb_start + sum(sha3_setup_cycles.values())

    dup_iterations[k] = [
        cpu_proto_t_sub_i,
        cpu_sha3_t_sub_i,
        cpu_other_non_accel_t_sub_i,

        acc_proto_t_sub_i,
        acc_proto_t_setup_i,

        acc_sha3_t_sub_i,
        acc_sha3_t_setup_i,
    ]

cpu_proto_t_sub_i = []
cpu_sha3_t_sub_i = []
cpu_other_non_accel_t_sub_i = []

acc_proto_t_sub_i = []
acc_proto_t_setup_i = []

acc_sha3_t_sub_i = []
acc_sha3_t_setup_i = []

for k, e in dup_iterations.items():
    cpu_proto_t_sub_i += [e[0]]
    cpu_sha3_t_sub_i += [e[1]]
    cpu_other_non_accel_t_sub_i += [e[2]]

    acc_proto_t_sub_i += [e[3]]
    acc_proto_t_setup_i += [e[4]]

    acc_sha3_t_sub_i += [e[5]]
    acc_sha3_t_setup_i += [e[6]]

print(f"Avg. CPU Protobuf Ser.    t_sub_i = {avg(cpu_proto_t_sub_i)}")
print(f"Avg. CPU SHA3 Hashing     t_sub_i = {avg(cpu_sha3_t_sub_i)}")
print(f"Avg. CPU Other Non-Accel. t_sub_i = {avg(cpu_other_non_accel_t_sub_i)}")
#print(f"acc_proto_t_sub_i   = {avg(acc_proto_t_sub_i)}")
#print(f"acc_proto_t_setup_i = {avg(acc_proto_t_setup_i)}")
#print(f"acc_sha3_t_sub_i    = {avg(acc_sha3_t_sub_i)}")
#print(f"acc_sha3_t_setup_i  = {avg(acc_sha3_t_setup_i)}")
