#pragma once

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%s:%d] " format " \n", \
    __FILE__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}
