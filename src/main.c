#include <stdio.h>
#include <stdint-gcc.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "mem_mapping.h"

int main() {
    int fd;
    struct stat filestat;
    void *p;
    fd = open("/home/vlad/Downloads/CentOS-7-x86_64-DVD-2009.iso", O_RDWR);
    if (fd < 0)
        exit(1);
    if (fstat(fd, &filestat) < 0)
        exit(2);

    p = mem_map(0, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)
        exit(3);
    for (uint64_t* pp = p; (void*)pp < p+sizeof(uint64_t)*10; pp++) {
        printf("%ld\n", *pp);
    }
    if (mem_unmap(p, filestat.st_size) != 0)
        exit(4);
    return 0;
}
