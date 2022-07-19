#pragma once

#include <cstdio>

extern "C" {
#include "libavutil/error.h"
}

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%d] " format " \n", \
   __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}

#define AV_LOG(code, format, ...)  	\
{								\
	char buf[1024] = { 0 };		\
	av_strerror(code, buf, 1023); \
    fprintf(stderr, "[%s:%d] " format " - %s. \n", \
   __FUNCTION__ , __LINE__, ##__VA_ARGS__, buf);     \
}
