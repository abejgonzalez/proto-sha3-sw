#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <cerrno>
#include <sys/mman.h>
#include <string>

// proto-specific
#include "primitives.pb.h"
#include "../accellib.h"

// sha-specific
#include "sha3.h"
#include "compiler.h"
#include "rocc.h"

// overall
#include "encoding.h"
#define NUM_ITERS 2
//#define PROTO_ACCEL
//#define SHA3_ACCEL
volatile char** ser_out_str_ptrs;
// make volatile to force whiles to not be opt.
volatile bool ser_inited = false;
volatile bool sha_finished = false;

// copied from: https://stackoverflow.com/questions/1407786/how-to-set-cpu-affinity-of-a-particular-pthread
// core_id = 0, 1, ... n-1, where n is the system's number of cores
int stick_this_thread_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id < 0 || core_id >= num_cores)
      return EINVAL;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

void* op_core0(void* arg) {
    stick_this_thread_to_core(0);
    printf("Core 0: Proto Serialization Working\n");
    unsigned int cpu, node, rc;
    if (rc = getcpu(&cpu, &node)) {
        printf("0: getcpu failed with rc=%d\n", rc);
    }
    printf("0: CPU=%d Node=%d\n", cpu, node);

    unsigned long setup_start, setup_end;
    unsigned long s_start[NUM_ITERS];
    unsigned long s_end[NUM_ITERS];

    // Setup proto objs
    google::protobuf::Arena arena;
    primitivetests::Paccser_boolMessage* proto_objs[NUM_ITERS];
    for (int i = 0; i < NUM_ITERS; i++) {
        proto_objs[i] = google::protobuf::Arena::CreateMessage<primitivetests::Paccser_boolMessage>(&arena);
        proto_objs[i]->set_paccbool_0(i % 2);
        proto_objs[i]->set_paccbool_1(i % 2);
        proto_objs[i]->set_paccbool_2(i % 2);
        proto_objs[i]->set_paccbool_3(i % 2);
        proto_objs[i]->set_paccbool_4(i % 2);
    }

    setup_start = rdcycle();
#ifdef PROTO_ACCEL
    // Setup str, and strptr memory regions (also touches all pages to avoid accel. pg. faults)
    ser_out_str_ptrs = AccelSetupSerializer();
#else
    // Setup output area for objs
    std::string out_strs[NUM_ITERS];
    const char* cpu_out_str_ptrs[NUM_ITERS];
    ser_out_str_ptrs = (volatile char**)(&cpu_out_str_ptrs);

    //printf("0: CPUInit: ser_out_str_ptrs:%p cpu_out_str_ptrs:%p\n", ser_out_str_ptrs, &cpu_out_str_ptrs);
#endif
    setup_end = rdcycle();

    ser_inited = true;

    for (int i = 0; i < NUM_ITERS; i++){
        s_start[i] = rdcycle();
#ifdef PROTO_ACCEL
        AccelSerializeToString(primitivetests, Paccser_boolMessage, proto_objs[i]);
        //BlockOnSerializedValue(ser_out_str_ptrs, i);
#else
        //for (int j = 0; j < NUM_ITERS; j++) {
        //    printf("0: ser_out_str_ptrs[%d]:%p cpu_out_str_ptrs[%d]:%p\n", ser_out_str_ptrs[i], cpu_out_str_ptrs[i]);
        //}
        //printf("0: out_strs[%d].length:%d cpu_out_str_ptrs[%d]:%p\n",
        //        i,
        //        out_strs[i].length(),
        //        i,
        //        cpu_out_str_ptrs[i]
        //        );
        //printf("0: -> Proto serialize\n");
        proto_objs[i]->SerializeToString(&out_strs[i]);
        cpu_out_str_ptrs[i] = out_strs[i].c_str();
        //printf("0: out_strs[%d].length:%d cpu_out_str_ptrs[%d](.length,val):%d,%p\n",
        //        i,
        //        out_strs[i].length(),
        //        i,
        //        strlen(cpu_out_str_ptrs[i]),
        //        cpu_out_str_ptrs[i]
        //        );
        //for (int j = 0; j < strlen(cpu_out_str_ptrs[i]); j++) {
        //    printf("0: cpu_out_str_ptrs[%d][%d]:0x%x\n",
        //            i,
        //            j,
        //            cpu_out_str_ptrs[i][j]
        //          );
        //}
#endif
        s_end[i] = rdcycle();
    }

    google::protobuf::ShutdownProtobufLibrary();

    // Wait for SHA3 to finish before exiting out of this thread (to prevent mem. dealloc)
    while (!sha_finished) {
        //printf("0: sha_finished:%d\n", sha_finished);
        //sleep(1);
    }

    printf("0: Setup=%d\n", setup_end - setup_start);
    for (int i = 0; i < NUM_ITERS; i++) {
        printf("0: Iter %d: Proto=%d\n", i, (s_end[i] - s_start[i]));
    }
    printf("0: SetupStart=%ld\n", setup_start);

    return 0;
}

void* op_core1(void* arg) {
    stick_this_thread_to_core(1);
    printf("Core 1: Working\n");
    unsigned int cpu, node, rc;
    if (rc = getcpu(&cpu, &node)) {
        printf("1: getcpu failed with rc=%d\n", rc);
    }
    printf("1: CPU=%d Node=%d\n", cpu, node);

    // Setup SHA3 output
    unsigned char sha3_output[NUM_ITERS][SHA3_256_DIGEST_SIZE] __aligned(8);

    unsigned long sha_start[NUM_ITERS];
    unsigned long sha_mid[NUM_ITERS];
    unsigned long sha_end[NUM_ITERS];

    while (!ser_inited) {
    //    printf("1: ser_inited:%d\n", ser_inited);
    //    sleep(1);
    }
    //printf("1: ser_out_str_ptrs:%p\n", ser_out_str_ptrs);

    for (int i = 0; i < NUM_ITERS; i++){
        sha_start[i] = rdcycle();

        // wait for ith iter of proto ser to finish
        while(ser_out_str_ptrs[i] == 0) {
            //for (int j = 0; j < NUM_ITERS; j++) {
            //    printf("1: ser_out_str_ptrs[%d]:%p\n", ser_out_str_ptrs[j]);
            //}
            //sleep(1);
        }

        sha_mid[i] = rdcycle();
#ifdef SHA3_ACCEL
        // Compute hash with accelerator
        asm volatile ("fence");
        // Invoke the acclerator and check responses

        // setup accelerator with addresses of input and output
        ROCC_INSTRUCTION_SS(2, &ser_out_str_ptrs[i], &sha3_output[i], 0);

        // Set length and compute hash
        ROCC_INSTRUCTION_S(2, strlen((const char*)ser_out_str_ptrs[i]), 1);
        asm volatile ("fence" ::: "memory");
#else
        //printf("1: ser_out_str_ptrs[%d](.length,val):%d,%p sha3_output[%d]:%p\n",
        //        i,
        //        strlen((const char*)ser_out_str_ptrs[i]),
        //        ser_out_str_ptrs[i],
        //        i,
        //        sha3_output[i]
        //        );
        //printf("1: --> SHA3 hash\n");
        sha3ONE((unsigned char*)ser_out_str_ptrs[i], strlen((const char*)ser_out_str_ptrs[i]), sha3_output[i]);
        //for (int j = 0; j < SHA3_256_DIGEST_SIZE; j++) {
        //    printf("1: sha3_output[%d][%d]:0x%x\n",
        //            i,
        //            j,
        //            sha3_output[i][j]
        //            );
        //}
#endif
        sha_end[i] = rdcycle();
    }

    sha_finished = true;

    for (int i = 0; i < NUM_ITERS; i++) {
        printf("1: Iter %d: SHAFull=%d SHACore=%d\n", i, (sha_end[i] - sha_start[i]), (sha_end[i] - sha_mid[i]));
    }
    printf("1: Last SHA counter: %ld\n", sha_end[NUM_ITERS - 1]);

    return 0;
}

int main() {
    // Ensure all pages are resident to avoid accelerator page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
        perror("mlockall");
        return 1;
    }

    int rc1, rc2;
    pthread_t tid0, tid1;

    if(rc1=pthread_create(&tid0, NULL, &op_core0, NULL)) {
       printf("Thread 0 creation failed: %d\n", rc1);
    }

    if(rc2=pthread_create(&tid1, NULL, &op_core1, NULL)) {
       printf("Thread 1 creation failed: %d\n", rc2);
    }

    pthread_join(tid0, NULL);
    pthread_join(tid1, NULL);

    printf("Success!\n\n");
}
