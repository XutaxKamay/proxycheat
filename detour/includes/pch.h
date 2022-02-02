#include <iostream>
#include <cstring>
#include <limits.h>
#include <memory>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <assert.h>

#include <linux/kernel.h>
#include <sys/syscall.h>

#if (__WORDSIZE == 64)
    #define MX64
#else
    #define MX86
#endif

#define asmv __asm__ __volatile__
