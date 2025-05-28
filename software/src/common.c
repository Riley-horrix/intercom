#include <sys/mman.h>
#include <time.h>
#include <common.h>

void* create_shared_memory(size_t size) {
    // Memory is readable and writeable
    int protection = PROT_READ | PROT_WRITE;

    // Allocated memory is shared but anonymous, so only this process and
    // children can access it.
    int visibility = MAP_SHARED | MAP_ANONYMOUS;

    return mmap(NULL, size, protection, visibility, -1, 0);
}

int destroy_shared_memory(void* memory, size_t size) {
    return munmap(memory, size) == 0 ? ST_GOOD : ST_FAIL;
}

static uint64_t get_ntime(void) {
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    return (uint64_t)tm.tv_sec * 1000000000 + (uint64_t)tm.tv_nsec;
}

uint64_t ntime(void) {
    static uint64_t tm_start = 0;

    if (tm_start == 0) {
        tm_start = get_ntime();
        return 0;
    }

    return get_ntime() - tm_start;
}