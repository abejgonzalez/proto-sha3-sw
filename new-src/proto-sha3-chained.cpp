#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string>

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
//volatile bool ser_inited = false;
//volatile bool sha_finished = false;
std::atomic<bool> sha_finished(false);
std::atomic<int> done_up_to(-1);
//pthread_mutex_t lock0 = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
#define ns_diff_s(x,y) (std::chrono::duration_cast<std::chrono::nanoseconds>((x) - (y)).count())

typedef struct pin_args {
    int cid;
} pin_args_t;

void* op_core0(void* arg) {
    pin_args_t* args = (pin_args_t*)arg;
    int PROTO_CID = args->cid;

    CHECK(stick_this_thread_to_core(PROTO_CID));
    CHECK(verify_cpu_pinned(PROTO_CID));
    //DEBUG_PRINT("PRO: Working on %d\n", PROTO_CID);

    //unsigned long accel_setup_start, accel_setup_end;
    //unsigned long s_start[NUM_ITERS];
    //unsigned long s_end[NUM_ITERS];
    //unsigned long setup_proto_start;
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

    //DEBUG_PRINT("PRO: AccelInit: ser_out_str_ptrs:%p\n", ser_out_str_ptrs);
#else
    // Setup output area for objs
    std::string out_strs[NUM_ITERS];
    const char* cpu_out_str_ptrs[NUM_ITERS];
    ser_out_str_ptrs = (volatile char**)(&cpu_out_str_ptrs);

    //DEBUG_PRINT("PRO: CPUInit: ser_out_str_ptrs:%p cpu_out_str_ptrs:%p\n", ser_out_str_ptrs, &cpu_out_str_ptrs);
#endif
    //accel_setup_end = rdcycle();
    auto accel_setup_end = std::chrono::steady_clock::now();

    //pthread_mutex_lock(&lock0);
    //ser_inited = true;
    //pthread_mutex_unlock(&lock0);
    //printf("PRO: Passed cond signal\n");
    //printf("PRO: rdcycle: %ld\n", rdcycle());

    for (int i = 0; i < NUM_ITERS; i++){
        //s_start[i] = rdcycle();
        auto s_start = std::chrono::steady_clock::now();
#ifdef PROTO_ACCEL
        //for (int j = 0; j < NUM_ITERS; j++) {
        //    DEBUG_PRINT("PRO: Pre: ser_out_str_ptrs[%d]:%p\n", j, ser_out_str_ptrs[j]);
        //}
        //DEBUG_PRINT("PRO: -> Proto serialize\n");
        //printf("PRO: -> Proto serialize %d\n", i);

        AccelSerializeToString(hyperprotobench, M52, proto_objs[i]);

        //printf("PRO: -> Done serialize %d\n", i);

        volatile char* str_ptr = BlockOnSerializedValue(ser_out_str_ptrs, i);
        done_up_to.store(i, std::memory_order_release);
        //printf("PRO: -> Done block %d\n", i);
        //DEBUG_PRINT("PRO: str_ptr[%d]:%p\n", i, str_ptr);
        //printf("PRO: ser_out_str_ptrs[%d]:%p\n", i, ser_out_str_ptrs[i]);
        //size_t str_len = GetSerializedLength(ser_out_str_ptrs, i);
        //DEBUG_PRINT("PRO: [%d] str_len:%d==%d\n", i, str_len, strlen((const char*)str_ptr));

        //for (int j = 0; j < NUM_ITERS; j++) {
        //    DEBUG_PRINT("PRO: Post: ser_out_str_ptrs[%d]:%p\n", j, ser_out_str_ptrs[j]);
        //}
#else
        //for (int j = 0; j < NUM_ITERS; j++) {
        //    DEBUG_PRINT("PRO: ser_out_str_ptrs[%d]:%p cpu_out_str_ptrs[%d]:%p\n", j, ser_out_str_ptrs[j], j, cpu_out_str_ptrs[j]);
        //}
        //DEBUG_PRINT("PRO: out_strs[%d].length:%d cpu_out_str_ptrs[%d]:%p\n",
        //        i,
        //        out_strs[i].length(),
        //        i,
        //        cpu_out_str_ptrs[i]
        //        );
        //DEBUG_PRINT("PRO: -> Proto serialize\n");
        proto_objs[i]->SerializeToString(&out_strs[i]);
        cpu_out_str_ptrs[i] = out_strs[i].c_str();
        //DEBUG_PRINT("PRO: out_strs[%d].length:%d cpu_out_str_ptrs[%d](.length,val):%d,%p\n",
        //        i,
        //        out_strs[i].length(),
        //        i,
        //        strlen(cpu_out_str_ptrs[i]),
        //        cpu_out_str_ptrs[i]
        //        );
        //for (int j = 0; j < strlen(cpu_out_str_ptrs[i]); j++) {
        //    DEBUG_PRINT("PRO: cpu_out_str_ptrs[%d][%d]:0x%x\n",
        //            i,
        //            j,
        //            cpu_out_str_ptrs[i][j]
        //          );
        //}
        //for (int j = 0; j < NUM_ITERS; j++) {
        //    DEBUG_PRINT("PRO: ser_out_str_ptrs[%d]:%p cpu_out_str_ptrs[%d]:%p\n", j, ser_out_str_ptrs[j], j, cpu_out_str_ptrs[j]);
        //}
#endif
        //s_end[i] = rdcycle();
        auto s_end = std::chrono::steady_clock::now();
        s_end_to_start[i] = ns_diff_s(s_end, s_start);
    }
    auto s_end_i = std::chrono::steady_clock::now();

    //printf("PRO: rdcycle: %ld\n", rdcycle());

    // Wait for SHA3 to finish before exiting out of this thread (to prevent mem. dealloc)
    //pthread_mutex_lock(&lock1);
    while (!sha_finished.load(std::memory_order_acquire)) {
    //    //printf("0: sha_finished:%d\n", sha_finished);
    //    //sleep(1);
    }
    //pthread_mutex_unlock(&lock1);

    //unsigned long print_start, print_end;
    //print_start = rdcycle();
    //printf("PRO: ProtoObjSetup=%lu\n", accel_setup_start - setup_proto_start);
    //printf("PRO: AccelSetup=%lu\n", accel_setup_end - accel_setup_start);
    //for (int i = 0; i < NUM_ITERS; i++) {
    //    printf("PRO: Iter %d: IterSerTime=%lu\n", i, (s_end[i] - s_start[i]));
    //}
    //printf("PRO: ProtoThreadOverall: %lu\n", s_end[NUM_ITERS - 1] - setup_proto_start);
    //print_end = rdcycle();
    //printf("PRO: P=%lu\n", print_end - print_start);

    auto print_start = std::chrono::steady_clock::now();
    printf("PRO: ProtoObjSetup=%lu\n", ns_diff_s(accel_setup_start, setup_proto_start));
    printf("PRO: AccelSetup=%lu\n", ns_diff_s(accel_setup_end, accel_setup_start));
    for (int i = 0; i < NUM_ITERS; i++) {
        //printf("PRO: Iter %lu: IterSerTime=%lu\n", i, (s_end[i] - s_start[i]));
        printf("PRO: Iter %lu: IterSerTime=%lu\n", i, s_end_to_start[i]);
    }
    //printf("PRO: BlockedTime=%lu\n", ns_diff_s(block_c, s_end_i));
    printf("PRO: ProtoThreadOverall=%lu\n", ns_diff_s(s_end_i, setup_proto_start));
    //print_end = rdcycle();
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
    //DEBUG_PRINT("SHA3: Working on %d\n", SHA3_CID);

    // Setup SHA3 output
    unsigned char sha3_output[NUM_ITERS][SHA3_256_DIGEST_SIZE] __aligned(8);

    ///unsigned long sha_start[NUM_ITERS];
    ///unsigned long sha_mid[NUM_ITERS];
    ///unsigned long sha_mid2[NUM_ITERS];
    ///unsigned long sha_end[NUM_ITERS];
    ///unsigned long start_thread;
    unsigned long sha_end_to_mid[NUM_ITERS];
    unsigned long sha_mid_to_start[NUM_ITERS];

    //start_thread = rdcycle();
    auto start_thread = std::chrono::steady_clock::now();

    //pthread_mutex_lock(&lock0);
    //while (!ser_inited) {
    ////    printf("1: ser_inited:%d\n", ser_inited);
    ////    sleep(1);
    //}
    //pthread_mutex_unlock(&lock0);
    //printf("SHA3: Start\n");
    //DEBUG_PRINT("SHA3: ser_out_str_ptrs:%p\n", ser_out_str_ptrs);
    //printf("SHA3: ser_out_str_ptrs:%p\n", ser_out_str_ptrs);

#ifdef SHA3_ACCEL
    // Clear accelerator TLB
    //DEBUG_PRINT("SHA3: Clear accel. TLB\n");
    asm volatile (".insn r CUSTOM_2, 0, 2, zero, zero, zero");
    //DEBUG_PRINT("SHA3: Done clearing accel. TLB\n");
#endif
    auto sha_start_0 = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_ITERS; i++){
        //sha_start[i] = rdcycle();
        auto sha_start = std::chrono::steady_clock::now();

        // wait for ith iter of proto ser to finish
        while(done_up_to.load(std::memory_order_acquire) < i) {
            asm volatile ("");
            //for (int j = 0; j < NUM_ITERS; j++) {
            //    printf("1: ser_out_str_ptrs[%d]:%p\n", ser_out_str_ptrs[j]);
            //}
            //sleep(1);
            //printf("SHA3: Wait %d < %d\n", done_up_to.load(std::memory_order_acquire), i);
        }
        //printf("SHA3: -> Ready to hash: %d ... %d\n", i, done_up_to.load(std::memory_order_acquire));

        //sha_mid[i] = rdcycle();
        auto sha_mid = std::chrono::steady_clock::now();
        if (ser_out_str_ptrs[i] != NULL) {
#ifdef SHA3_ACCEL
            //printf("SHA3: ser_out_str_ptrs[%d](.length,val):%d,%p sha3_output[%d]:%p\n",
            //        i,
            //        strlen((const char*)ser_out_str_ptrs[i]),
            //        ser_out_str_ptrs[i],
            //        i,
            //        sha3_output[i]
            //        );
            ////DEBUG_PRINT("SHA3: --> SHA3 hash\n");
            //printf("SHA3: -> SHA3 hash setup: %d\n", i);

            asm volatile ("fence");

            // Setup accelerator with addresses of input and output
            ROCC_INSTRUCTION_SS(2, &ser_out_str_ptrs[i], &sha3_output[i], 0);

            // Set length and compute hash
            ROCC_INSTRUCTION_S(2, strlen((const char*)ser_out_str_ptrs[i]), 1);

            asm volatile ("fence" ::: "memory");

            //for (int j = 0; j < SHA3_256_DIGEST_SIZE; j++) {
            //    DEBUG_PRINT("SHA3: sha3_output[%d][%d]:0x%x\n",
            //            i,
            //            j,
            //            sha3_output[i][j]
            //            );
            //}
#else
            //DEBUG_PRINT("SHA3: ser_out_str_ptrs[%d](.length,val):%d,%p sha3_output[%d]:%p\n",
            //        i,
            //        strlen((const char*)ser_out_str_ptrs[i]),
            //        ser_out_str_ptrs[i],
            //        i,
            //        sha3_output[i]
            //        );
            //DEBUG_PRINT("SHA3: --> SHA3 hash\n");
            //printf("SHA3: -> SHA3 hash %d\n", i);
            sha3ONE((unsigned char*)ser_out_str_ptrs[i], strlen((const char*)ser_out_str_ptrs[i]), sha3_output[i]);
            //for (int j = 0; j < SHA3_256_DIGEST_SIZE; j++) {
            //    DEBUG_PRINT("SHA3: sha3_output[%d][%d]:0x%x\n",
            //            i,
            //            j,
            //            sha3_output[i][j]
            //            );
            //}
#endif
            //printf("SHA3: -> SHA3 complete hash: %d\n", i);
        } else {
            printf("SHA3: Skipped %d\n", i);
        }

        //sha_end[i] = rdcycle();
        auto sha_end = std::chrono::steady_clock::now();
        sha_end_to_mid[i] = ns_diff_s(sha_end, sha_mid);
        sha_mid_to_start[i] = ns_diff_s(sha_mid, sha_start);
    }

    auto sha_end_i = std::chrono::steady_clock::now();

    //pthread_mutex_lock(&lock1);
    sha_finished.store(true, std::memory_order_release);
    //pthread_mutex_unlock(&lock1);

    //unsigned long print_start, print_end;
    //print_start = rdcycle();
    //for (int i = 0; i < NUM_ITERS; i++) {
    //    printf("SHA3: Iter %d: SHAFull=%lu SHACore=%lu SHASetup=%lu\n", i, (sha_end[i] - sha_start[i]), (sha_end[i] - sha_mid[i]), (sha_mid2[i] - sha_mid[i]));
    //}
    //printf("SHA3: LoopCycles: %lu\n", sha_end[NUM_ITERS - 1] - sha_start[0]);
    //printf("SHA3: OverallThreadCycles: %lu\n", sha_end[NUM_ITERS - 1] - start_thread);
    //print_end = rdcycle();
    //printf("SHA3: P=%lu\n", print_end - print_start);

    auto print_start = std::chrono::steady_clock::now();
    for (int i = 0; i < NUM_ITERS; i++) {
        //printf("SHA3: Iter %d: SHAFull=%lu SHACore=%lu SHASetup=%lu\n", i, (sha_end[i] - sha_start[i]), (sha_end[i] - sha_mid[i]), (sha_mid[i] - sha_start[i]));
        printf("SHA3: Iter %d: SHACore=%lu SHAWait=%lu\n", i, sha_end_to_mid[i], sha_mid_to_start[i]);
    }
    printf("SHA3: LoopCycles: %lu\n", ns_diff_s(sha_end_i, sha_start_0));
    printf("SHA3: OverallThreadCycles: %lu\n", ns_diff_s(sha_end_i, start_thread));
    //print_end = rdcycle();
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

    //unsigned long o_start[NUM_DUPS];
    //unsigned long o_end[NUM_DUPS];
    unsigned long ns_diff[NUM_DUPS];

    int rc1, rc2;
    pthread_t tid0, tid1;

    for (int i = 0; i < NUM_DUPS; i++) {
        printf("Dup Itr: %d\n", i);
        //o_start[i] = rdcycle();
        auto oo_start = std::chrono::steady_clock::now();

        sha_finished.store(false);
        done_up_to.store(-1);

        if(rc1=pthread_create(&tid0, NULL, &op_core0, &tid0_pin_args)) {
            DEBUG_PRINT("Thread 0 creation failed: %d\n", rc1);
        }

        if(rc2=pthread_create(&tid1, NULL, &op_core1, &tid1_pin_args)) {
            DEBUG_PRINT("Thread 1 creation failed: %d\n", rc2);
        }

        pthread_join(tid0, NULL);
        pthread_join(tid1, NULL);

        //o_end[i] = rdcycle();
        auto oo_end = std::chrono::steady_clock::now();
        ns_diff[i] = ns_diff_s(oo_end, oo_start);
    }

    for (int i = 0; i < NUM_DUPS; i++) {
        //printf("Overall[%d]: %lu = %lu - %lu\n", i, o_end[i] - o_start[i], o_end[i], o_start[i]);
        printf("Overall[%d]: %lu\n", i, ns_diff[i]);
    }

    DEBUG_PRINT("Success!\n\n");
}
