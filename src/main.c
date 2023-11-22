#include <stdio.h>
#include <stdint-gcc.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <memory.h>
#include "mem_mapping.h"

int main() {
    int fd;
    struct stat filestat;
    fd = open("/home/vlad/Documents/123", O_RDWR);
    if (fd < 0)
        exit(1);
    if (fstat(fd, &filestat) < 0)
        exit(2);

//    uint8_t* pw = mem_map(0, filestat.st_size, PROT_READ, MAP_SHARED, fd, 0);

//    printf("%ld", filestat.st_size);
    int offset = 1;
    uint8_t* pw = mem_map(0, filestat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, offset);
    uint8_t* pr = mem_map(0, filestat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    char* str = "\n<<CLOWN1 MESSAGE>>\n\0";
    strcpy((char*)pr, str);
    strcpy((char*)pw, str);
    strcpy((char*)pr+1043, str);

    if (pw == MAP_FAILED || pr == MAP_FAILED)
        exit(3);
    for (uint8_t* pp = pr; (int64_t)(pp - pr) < filestat.st_size; pp++) {
        printf("%c", *pp);
    }
    if (mem_unmap(pw, filestat.st_size) != 0 || mem_unmap(pr, filestat.st_size) != 0)
        exit(4);
    return 0;
}
