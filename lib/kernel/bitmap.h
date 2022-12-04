#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include "global.h"

#define BITMAP_MASK 1

typedef struct __bitmap_t{
    uint32_t btmp_byte_len;             // 位图的长度，以字节位长度
    uint8_t *bits;                      // 指向位图的指针
} bitmap_t;

/**
 * @brief 位图bitmap的初始化函数
 * 
 * @param btmp 指向需要初始化的bitmap的指针
 */
void bitmap_init(bitmap_t* btmp);


/**
 * @brief 判断位图中指定的位是否位1
 * 
 * @param btmp 指向bitmap的指针
 * @param bit_idx 需要判断的位
 * @return true 1
 * @return false 0
 */
bool bitmap_scan_test(bitmap_t *btmp, uint32_t bit_idx);


/**
 * @brief 在位图中申请连续的cnt个位，若成功则返回起始位的下标，失败则返回-1
 * 
 * @param btmp 指向bitmap的指针
 * @param cnt 连续的位数
 * @return int 起始位的下标
 */
int bitmap_scan(bitmap_t *btmp, uint32_t cnt);


/**
 * @brief 将bitmap中的bit_idx位设置为value
 * 
 * @param btmp 指向bitmap的指针
 * @param bit_idx 要设置的位的索引
 * @param value 要设置的值
 */
void bitmap_set(bitmap_t *btmp, uint32_t bit_idx, int8_t value);

#endif