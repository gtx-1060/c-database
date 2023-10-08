//
// Created by vlad on 28.09.23.
//

// TODO: make it depends on system page size (_SC_PAGESIZE in unix)
#define SYS_PAGE_SIZE 4096

#if defined(__LP64__) || defined(_LP64)
    #define BUILD_64
#endif

#if defined(__GNUC__) || defined(__GNUC__)
    #include <stdint-gcc.h>
#elif defined(_MSC_VER)

#endif

