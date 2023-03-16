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
#define SERITERS 1
#define NUMTESTVALS 5
//#define proto_accel
//#define sha3_accel

int main() {
    // Ensure all pages are resident to avoid accelerator page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE)) {
        perror("mlockall");
        return 1;
    }

    volatile char** serializeoutputs;

    bool testvals[NUMTESTVALS] = {true, true, true, true, true};

    google::protobuf::Arena arena;

    primitivetests::Paccser_boolMessage* parseintos[SERITERS];
    for (int q = 0; q < SERITERS; q++) {
        parseintos[q] = google::protobuf::Arena::CreateMessage<primitivetests::Paccser_boolMessage>(&arena);

        parseintos[q]->set_paccbool_0(testvals[0]);
        parseintos[q]->set_paccbool_1(testvals[1]);
        parseintos[q]->set_paccbool_2(testvals[2]);
        parseintos[q]->set_paccbool_3(testvals[3]);
        parseintos[q]->set_paccbool_4(testvals[4]);
    }

#ifdef proto_accel
    // Setup str, and strptr memory regions (also touches all pages to avoid accel. pg. faults)
    serializeoutputs = AccelSetupSerializer();

    for (int i = 0; i < SERITERS; i++) {
        AccelSerializeToString(primitivetests, Paccser_boolMessage, parseintos[i]);
    }

    for (int i = 0; i < SERITERS; i++) {
        BlockOnSerializedValue(serializeoutputs, i);
    }
#else
    // Setup output area for objs
    std::string output_strings[SERITERS];
    const char* output_string_ptrs[SERITERS];

    for (int i = 0; i < SERITERS; i++) {
        parseintos[i]->SerializeToString(&output_strings[i]);
    }

    serializeoutputs = (volatile char**)(&output_string_ptrs);
    for (int i = 0; i < SERITERS; i++) {
        output_string_ptrs[i] = output_strings[i].c_str();
    }
#endif

    google::protobuf::ShutdownProtobufLibrary();

    // Setup SHA3 output
    unsigned char output[SHA3_256_DIGEST_SIZE][SERITERS] __aligned(8);

#ifdef sha_accel
    for (int i = 0; i < SERITERS; i++) {
        // Compute hash with accelerator
        asm volatile ("fence");
        // Invoke the acclerator and check responses

        // setup accelerator with addresses of input and output
        ROCC_INSTRUCTION_SS(2, &serializeoutputs[i], &output[i], 0);

        // Set length and compute hash
        ROCC_INSTRUCTION_S(2, sizeof(serializeoutputs[i]), 1);
        asm volatile ("fence" ::: "memory");
    }
#else
    for (int i = 0; i < SERITERS; i++) {
        sha3ONE((unsigned char*)serializeoutputs[i], sizeof(serializeoutputs[i]), output[i]);
    }
#endif

    printf("Success!\n");

    return 0;
}
