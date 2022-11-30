#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string>

// sha-specific
//#include "sha3.h"
//#include "compiler.h"
//#include "rocc.h"

// overall
#include "utils.h"
#include "encoding.h"

void* op_core0(void* arg) {
    printf("Thread 0: Run custom instruction\n");
    asm volatile (".insn r CUSTOM_2, 0, 2, zero, zero, zero");
    printf("Thread 0: Run custom instruction finished\n");
    return 0;
}

void* op_core1(void* arg) {
    printf("Thread 1: Run custom instruction\n");
    asm volatile (".insn r CUSTOM_2, 0, 2, zero, zero, zero");
    printf("Thread 1: Run custom instruction finished\n");
    return 0;
}

int main(int argc, char* argv[]) {
    DEBUG_PRINT("Starting test!\n\n");

    int rc1, rc2;
    pthread_t tid0, tid1;

    if(rc1=pthread_create(&tid0, NULL, &op_core0, NULL)) {
        DEBUG_PRINT("Thread 0 creation failed: %d\n", rc1);
    }

    if(rc2=pthread_create(&tid1, NULL, &op_core1, NULL)) {
        DEBUG_PRINT("Thread 1 creation failed: %d\n", rc2);
    }

    pthread_join(tid0, NULL);
    pthread_join(tid1, NULL);

    DEBUG_PRINT("Success!\n\n");
}
