#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef OSX
    #include <malloc/malloc.h>
#else
    #include <malloc.h>
#endif
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <new>

typedef unsigned char uint8;
typedef signed char int8;
typedef short int16;
typedef unsigned short uint16;
typedef int int32;
typedef unsigned int uint32;
typedef long int64;
typedef unsigned long uint64;

#endif
