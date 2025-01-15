#pragma once

struct sbiret {
    long error;
    long value;
};
// The outer loop is used to combine multiple statements into a macro
#define PANIC(fmt, ...) \
    do { \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        while (1) {} \
    } while (0) 
