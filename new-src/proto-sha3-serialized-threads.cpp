#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#include <chrono>

// proto-specific
#include "benchmark.pb.h"
#include "../accellib.h"

bool do_write = false;
namespace hyperprotobench {
#include "benchmark.inc"
}

// sha-specific
#include "sha3.h"
#include "compiler.h"
#include "rocc.h"

// overall
#include "utils.h"
#include "encoding.h"
#define NUM_ITERS 10
#define NUM_DUPS 50
//#define PROTO_ACCEL
//#define SHA3_ACCEL
volatile char** volatile ser_out_str_ptrs;
// make volatile to force whiles to not be opt.
std::atomic<bool> ser_inited(false);
std::atomic<bool> sha_finished(false);
#define ns_diff_s(x,y) (std::chrono::duration_cast<std::chrono::nanoseconds>((x) - (y)).count())

typedef struct pin_args {
    int cid;
} pin_args_t;

void* op_core0(void* arg) {
    pin_args_t* args = (pin_args_t*)arg;
    int PROTO_CID = args->cid;

    CHECK(stick_this_thread_to_core(PROTO_CID));
    CHECK(verify_cpu_pinned(PROTO_CID));
    unsigned long s_end_to_start[NUM_ITERS];

    //setup_proto_start = rdcycle();
    auto setup_proto_start = std::chrono::steady_clock::now();

    // Setup proto objs
    google::protobuf::Arena arena;
    hyperprotobench::M52* proto_objs[NUM_ITERS];
    for (int i = 0; i < NUM_ITERS; i++) {
        proto_objs[i] = google::protobuf::Arena::CreateMessage<hyperprotobench::M52>(&arena);
        M52_Set_F1(proto_objs[i], NULL, NULL);
    }

    //accel_setup_start = rdcycle();
    auto accel_setup_start = std::chrono::steady_clock::now();
#ifdef PROTO_ACCEL
    // Setup str, and strptr memory regions (also touches all pages to avoid accel. pg. faults)
    ser_out_str_ptrs = AccelSetupSerializer();
#else
    // Setup output area for objs
    std::string out_strs[NUM_ITERS];
    const char* cpu_out_str_ptrs[NUM_ITERS];
    ser_out_str_ptrs = (volatile char**)(&cpu_out_str_ptrs);
#endif
    //accel_setup_end = rdcycle();
    auto accel_setup_end = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_ITERS; i++){
        auto p_start = std::chrono::steady_clock::now();
#ifdef PROTO_ACCEL
        AccelSerializeToString(hyperprotobench, M52, proto_objs[i]);
#else
        proto_objs[i]->SerializeToString(&out_strs[i]);
        cpu_out_str_ptrs[i] = out_strs[i].c_str();
#endif
        auto p_end = std::chrono::steady_clock::now();
        s_end_to_start[i] = ns_diff_s(p_end, p_start);
    }

    auto s_end_i = std::chrono::steady_clock::now();

#if PROTO_ACCEL
    volatile char* str_ptr = BlockOnSerializedValue(ser_out_str_ptrs, NUM_ITERS-1);
#endif

    auto block_c = std::chrono::steady_clock::now();

    ser_inited.store(true, std::memory_order_release);

    // Wait for SHA3 to finish before exiting out of this thread (to prevent mem. dealloc)
    while (!sha_finished.load(std::memory_order_acquire)) {
    }

    auto print_start = std::chrono::steady_clock::now();
    printf("PRO: ProtoObjSetup=%lu\n", ns_diff_s(accel_setup_start, setup_proto_start));
    printf("PRO: AccelSetup=%lu\n", ns_diff_s(accel_setup_end, accel_setup_start));
    for (int i = 0; i < NUM_ITERS; i++) {
        printf("PRO: Iter %lu: IterSerTime=%lu\n", i, s_end_to_start[i]);
    }
    printf("PRO: BlockedTime=%lu\n", ns_diff_s(block_c, s_end_i));
    printf("PRO: ProtoThreadOverall=%lu\n", ns_diff_s(s_end_i, setup_proto_start));
    auto print_end = std::chrono::steady_clock::now();
    printf("PRO: P=%lu\n", ns_diff_s(print_end, print_start));

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}

void* op_core1(void* arg) {
    pin_args_t* args = (pin_args_t*)arg;
    int SHA3_CID = args->cid;

    CHECK(stick_this_thread_to_core(SHA3_CID));
    CHECK(verify_cpu_pinned(SHA3_CID));

    // Setup SHA3 output
    unsigned char sha3_output[NUM_ITERS][SHA3_256_DIGEST_SIZE] __aligned(8);

    unsigned long sha_end_to_mid[NUM_ITERS];
    unsigned long sha_mid_to_start[NUM_ITERS];

    auto start_thread = std::chrono::steady_clock::now();

    while (!ser_inited.load(std::memory_order_acquire)) {
    }

    auto pre_tlb = std::chrono::steady_clock::now();

#ifdef SHA3_ACCEL
    // Clear accelerator TLB
    asm volatile (".insn r CUSTOM_2, 0, 2, zero, zero, zero");
#endif

    auto sha_start_0 = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_ITERS; i++){
        auto sha_start = std::chrono::steady_clock::now();
#ifdef SHA3_ACCEL
        asm volatile ("fence");

        // Setup accelerator with addresses of input and output
        ROCC_INSTRUCTION_SS(2, &ser_out_str_ptrs[i], &sha3_output[i], 0);
#endif
        auto sha_mid = std::chrono::steady_clock::now();
#ifdef SHA3_ACCEL
        // Set length and compute hash
        ROCC_INSTRUCTION_S(2, strlen((const char*)ser_out_str_ptrs[i]), 1);

        asm volatile ("fence" ::: "memory");

#else
        sha3ONE((unsigned char*)ser_out_str_ptrs[i], strlen((const char*)ser_out_str_ptrs[i]), sha3_output[i]);
#endif
        auto sha_end = std::chrono::steady_clock::now();
        sha_end_to_mid[i] = ns_diff_s(sha_end, sha_mid);
        sha_mid_to_start[i] = ns_diff_s(sha_mid, sha_start);
    }

    auto sha_end_i = std::chrono::steady_clock::now();

    sha_finished.store(true, std::memory_order_release);

    //unsigned long print_start, print_end;
    auto print_start = std::chrono::steady_clock::now();
    for (int i = 0; i < NUM_ITERS; i++) {
        printf("SHA3: Iter %d: SHAFull=%lu SHACore=%lu SHASetup=%lu\n", i, sha_end_to_mid[i] + sha_mid_to_start[i], sha_end_to_mid[i], sha_mid_to_start[i]);
    }
    printf("SHA3: Start->PreTLB: %lu\n", ns_diff_s(pre_tlb, start_thread));
    printf("SHA3: PreTLB->Start: %lu\n", ns_diff_s(sha_start_0, pre_tlb));
    printf("SHA3: LoopCycles: %lu\n", ns_diff_s(sha_end_i, sha_start_0));
    printf("SHA3: OverallThreadCycles: %lu\n", ns_diff_s(sha_end_i, start_thread));
    auto print_end = std::chrono::steady_clock::now();
    printf("SHA3: P=%lu\n", ns_diff_s(print_end, print_start));

    return 0;
}

int main(int argc, char* argv[]) {
    DEBUG_PRINT("Starting test!\n\n");

    if (argc != 3) {
        fprintf(stderr, "invalid # args: %s PROTO_CID SHA3_CID\n", argv[0]);
        exit(1);
    }

    pin_args_t tid0_pin_args;
    tid0_pin_args.cid = std::stoi(argv[1]);
    pin_args_t tid1_pin_args;
    tid1_pin_args.cid = std::stoi(argv[2]);

    // Ensure all pages are resident to avoid accelerator page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
        perror("mlockall");
        return 1;
    }

    unsigned long ns_diff[NUM_DUPS];

    int rc1, rc2;
    pthread_t tid0, tid1;

    for (int i = 0; i < NUM_DUPS; i++) {
        printf("Dup Itr: %d\n", i);
        auto oo_start = std::chrono::steady_clock::now();

        sha_finished.store(false);
        ser_inited.store(false);

        if(rc1=pthread_create(&tid0, NULL, &op_core0, &tid0_pin_args)) {
            DEBUG_PRINT("Thread 0 creation failed: %d\n", rc1);
        }

        if(rc2=pthread_create(&tid1, NULL, &op_core1, &tid1_pin_args)) {
            DEBUG_PRINT("Thread 1 creation failed: %d\n", rc2);
        }

        pthread_join(tid0, NULL);
        pthread_join(tid1, NULL);

        auto oo_end = std::chrono::steady_clock::now();
        ns_diff[i] = ns_diff_s(oo_end, oo_start);
    }

    for (int i = 0; i < NUM_DUPS; i++) {
        printf("Overall[%d]: %lu\n", i, ns_diff[i]);
    }

    DEBUG_PRINT("Success!\n\n");
}
