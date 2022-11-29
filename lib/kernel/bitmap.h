#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include "global.h"

#define BITMAP_MASK 1

typedef struct __bitmap_t{
    uint32_t btmp_byte_len;             // 位图的长度，以字节位长度
    uint8_t *bits;                      // 指向位图的指针
} bitmap_t;

void bitmap_init(bitmap_t* btmp);
bool bitmap_scan_test(bitmap_t *btmp, uint32_t bit_idx);
int bitmap_scan(bitmap_t *btmp, uint32_t cnt);
void bitmap_set(bitmap_t *btmp, uint32_t bit_idx, int8_t value);

#endif