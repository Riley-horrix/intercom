#include <sys/mman.h>
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