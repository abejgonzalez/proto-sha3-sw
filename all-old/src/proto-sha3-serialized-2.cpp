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
#define NUM_ITERS 4
//#define PROTO_ACCEL
//#define SHA3_ACCEL

int main() {
    // Ensure all pages are resident to avoid accelerator page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
        perror("mlockall");
        return 1;
    }

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

    // Setup ser output str ptr
    volatile char** ser_out_str_ptrs;

    // Setup SHA3 output
    unsigned char sha3_output[NUM_ITERS][SHA3_256_DIGEST_SIZE] __aligned(8);

    // Setup measurements
    unsigned long p_start[NUM_ITERS];
    unsigned long s_start[NUM_ITERS];
    unsigned long s_end[NUM_ITERS];
    unsigned long setup_start, setup_end;

    setup_start = rdcycle();
#ifdef PROTO_ACCEL
    // Setup str, and strptr memory regions (also touches all pages to avoid accel. pg. faults)
    ser_out_str_ptrs = AccelSetupSerializer();
#else
    // Setup output area for objs
    std::string out_strs[NUM_ITERS];
    const char* cpu_out_str_ptrs[NUM_ITERS];
    ser_out_str_ptrs = (volatile char**)(&cpu_out_str_ptrs);

    //printf("CPUInit: ser_out_str_ptrs:%p cpu_out_str_ptrs:%p\n", ser_out_str_ptrs, &cpu_out_str_ptrs);
#endif
    setup_end = rdcycle();

    for (int i = 0; i < NUM_ITERS; i++){
        p_start[i] = rdcycle();
#ifdef PROTO_ACCEL
        AccelSerializeToString(primitivetests, Paccser_boolMessage, proto_objs[i]);
        BlockOnSerializedValue(ser_out_str_ptrs, i);
#else
        //printf("out_strs[%d].length:%d cpu_out_str_ptrs[%d]:%p\n",
        //        i,
        //        out_strs[i].length(),
        //        i,
        //        cpu_out_str_ptrs[i]
        //        );
        //printf("-> Proto serialize\n");
        proto_objs[i]->SerializeToString(&out_strs[i]);
        cpu_out_str_ptrs[i] = out_strs[i].c_str();
        //printf("out_strs[%d].length:%d cpu_out_str_ptrs[%d](.length,val):%d,%p\n",
        //        i,
        //        out_strs[i].length(),
        //        i,
        //        strlen(cpu_out_str_ptrs[i]),
        //        cpu_out_str_ptrs[i]
        //        );
        //for (int j = 0; j < strlen(cpu_out_str_ptrs[i]); j++) {
        //    printf("cpu_out_str_ptrs[%d][%d]:0x%x\n",
        //            i,
        //            j,
        //            cpu_out_str_ptrs[i][j]
        //          );
        //}
#endif
        s_start[i] = rdcycle();
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
        //printf("ser_out_str_ptrs[%d](.length,val):%d,%p sha3_output[%d]:%p\n",
        //        i,
        //        strlen((const char*)ser_out_str_ptrs[i]),
        //        ser_out_str_ptrs[i],
        //        i,
        //        sha3_output[i]
        //        );
        //printf("--> SHA3 hash\n");
        sha3ONE((unsigned char*)ser_out_str_ptrs[i], strlen((const char*)ser_out_str_ptrs[i]), sha3_output[i]);
        //for (int j = 0; j < SHA3_256_DIGEST_SIZE; j++) {
        //    printf("sha3_output[%d][%d]:0x%x\n",
        //            i,
        //            j,
        //            sha3_output[i][j]
        //            );
        //}
#endif
        s_end[i] = rdcycle();
    }

    google::protobuf::ShutdownProtobufLibrary();

    printf("Success!\n\n");

    printf("Setup=%d\n", setup_end - setup_start);
    for (int i = 0; i < NUM_ITERS; i++) {
        printf("Iter %d: Proto=%d SHA3=%d\n", i, (s_start[i] - p_start[i]), (s_end[i] - s_start[i]));
    }
    printf("E2E=%d\n", s_end[NUM_ITERS-1] - setup_start);

    return 0;
}
