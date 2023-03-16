#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <cerrno>

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

typedef struct core0_args {
    int my_fancy_arg;
} core0_args_t;

void* op_core0(void* arg) {
   stick_this_thread_to_core(0);
   printf("Core 0: Working\n");
   unsigned int cpu, node, rc;
   if (rc = getcpu(&cpu, &node)) {
       printf("0: getcpu failed with rc=%d\n", rc);
   }
   printf("0: CPU=%d Node=%d\n", cpu, node);

   core0_args_t* c0_args = (core0_args_t*)arg;
   printf("0: my_fancy_arg=0x%x\n", c0_args->my_fancy_arg);
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
   return 0;
}

int main(void) {
    int rc1, rc2;
    pthread_t thread1, thread2;
    core0_args_t c0_args;
    c0_args.my_fancy_arg = 0xDEADBEEF;

    /* Create independent threads each of which will execute functionC */

    if(rc1=pthread_create(&thread1, NULL, &op_core0, &c0_args)) {
       printf("Thread creation failed: %d\n", rc1);
    }

    if(rc2=pthread_create(&thread2, NULL, &op_core1, NULL)) {
       printf("Thread creation failed: %d\n", rc2);
    }

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
}


