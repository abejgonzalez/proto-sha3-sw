#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <cerrno>

#ifdef DEBUG
 #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, \
             __FILE__, __LINE__, __func__, ##args)
#else
 #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif

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

int verify_cpu_pinned(int core_id) {
    unsigned int cpu, node, rc;
    if (rc = getcpu(&cpu, &node)) {
        DEBUG_PRINT("getcpu failed for cid:%d check with rc=%d\n", core_id, rc);
        return rc;
    }
    DEBUG_PRINT("cpu=%d core_id=%d is_equal=%d\n", cpu, core_id, cpu == core_id);
    return cpu != core_id;
}

#define CHECK(x) \
    do { \
        int retval = (x); \
        if (retval != 0) { \
            DEBUG_PRINT("ERROR: %s returned %d at %s:%d\n", #x, retval, __FILE__, __LINE__); \
            exit(retval); \
        } \
    } while (0)

#endif //UTILS_H
