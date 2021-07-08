#pragma once

#include <cstdio>

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%d] " format " \n", \
     __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}
