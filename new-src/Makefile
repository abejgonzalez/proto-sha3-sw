# requires the proto folder to be completely setup

RISCV_PREFIX=riscv64-unknown-linux-gnu
CXX=$(RISCV_PREFIX)-g++

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

CXXFLAGS=-std=c++11 -O3 -g3 -static -pthread $(EXTRA_CXXFLAGS)

HYPERSCALE_REPO=$(ROOT_DIR)/../../../target-design/chipyard

########## PROTO ##########

PROTO_ACC_DIR=$(HYPERSCALE_REPO)/generators/protoacc

PROTO_CXX_SRCS = \
	$(PROTO_ACC_DIR)/microbenchmarks/accellib.cpp \
	$(PROTO_ACC_DIR)/firesim-workloads/hyperproto/HyperProtoBench/bench0-ser/benchmark.pb.cc

PROTO_CXX_HDRS = \
	$(PROTO_ACC_DIR)/microbenchmarks/accellib.h \
	$(PROTO_ACC_DIR)/firesim-workloads/hyperproto/HyperProtoBench/bench0-ser/benchmark.pb.h \
	$(PROTO_ACC_DIR)/firesim-workloads/hyperproto/HyperProtoBench/bench0-ser/benchmark.inc

PROTO_CXX_ALL = $(PROTO_CXX_SRCS) $(PROTO_CXX_HDRS)

PROTO_LIB = $(PROTO_ACC_DIR)/microbenchmarks/protobuf-riscv-install/lib/libprotobuf.a
PROTO_LIB_HDR_DIR = $(PROTO_ACC_DIR)/microbenchmarks/protobuf-riscv-install/include

########## SHA ##########

SHA_DIR=$(HYPERSCALE_REPO)/generators/sha3
SHA_SW_DIR=$(SHA_DIR)/software/benchmarks/src

SHA_CXX_HDRS = \
	$(SHA_SW_DIR)/compiler.h \
	$(SHA_SW_DIR)/sha3.h

SHA_CXX_ALL = $(SHA_CXX_SRCS) $(SHA_CXX_HDRS)

########## ROCC ##########

# use this as the single src of truth rocc.h
ROCC_CXX_HDRS = \
	$(PROTO_ACC_DIR)/microbenchmarks/rocc.h

########## MISC ##########

# use this as the single src of truth encoding.h
MISC_CXX_HDRS = \
	$(SHA_SW_DIR)/encoding.h \
	$(ROOT_DIR)/utils.h

PROTO_HDR_DIRS += $(sort $(foreach filename,$(PROTO_CXX_HDRS),$(dir $(filename))))
SHA_HDR_DIRS += $(sort $(foreach filename,$(SHA_CXX_HDRS),$(dir $(filename))))
ROCC_HDR_DIRS += $(sort $(foreach filename,$(ROCC_CXX_HDRS),$(dir $(filename))))
MISC_HDR_DIRS += $(sort $(foreach filename,$(MISC_CXX_HDRS),$(dir $(filename))))
CXXFLAGS += \
	$(foreach dirname,$(PROTO_HDR_DIRS),-I $(dirname)) \
	$(foreach dirname,$(SHA_HDR_DIRS),-I $(dirname)) \
	$(foreach dirname,$(ROCC_HDR_DIRS),-I $(dirname)) \
	$(foreach dirname,$(MISC_HDR_DIRS),-I $(dirname)) \
	-I $(PROTO_LIB_HDR_DIR)

#TESTS=multithreaded-test proto-sha3-serialized proto-sha3-serialized-2 proto-sha3-chained
TESTS=proto-sha3-serialized-threads proto-sha3-chained
COMPILED_TESTS=$(addsuffix .riscv,$(TESTS))

default: $(COMPILED_TESTS)

#%.riscv: %.cpp $(PROTO_CXX_ALL) $(SHA_CXX_ALL) $(ROCC_CXX_HDRS)
%.riscv: %.cpp
	$(CXX) $(CXXFLAGS) $< $(PROTO_CXX_SRCS) $(PROTO_LIB) -pthread -o $@

clean:
	rm -rf $(COMPILED_TESTS)
