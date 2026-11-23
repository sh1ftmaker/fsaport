#include <stdio.h>
#include <stdlib.h>
void* test_malloc(unsigned long n) { return malloc(n); }
void test_free(void* p) { free(p); }
void* test_open(const char* path) { return fopen(path, "rb"); }
int test_read(void* f, unsigned int offset, void* buf, unsigned int size)
{
    fseek((FILE*)f, (long)offset, SEEK_SET);
    return fread(buf, 1, size, (FILE*)f) == size;
}
