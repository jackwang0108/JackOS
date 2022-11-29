#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

/**
 * @brief 位图bitmap的初始化函数
 * 
 * @param btmp 指向需要初始化的bitmap的指针
 */
void bitmap_init(bitmap_t *btmp){
    memset(btmp->bits, 0, btmp->btmp_byte_len);
}

/**
 * @brief 判断位图中指定的位是否位1
 * 
 * @param btmp 指向bitmap的指针
 * @param bit_idx 需要判断的位
 * @return true 1
 * @return false 0
 */
bool bitmap_scan_test(bitmap_t *btmp, uint32_t bit_idx){
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;
    return (btmp->bits[byte_idx]) & (BITMAP_MASK << bit_odd);
}


/**
 * @brief 在位图中申请连续的cnt个位，若成功则返回起始位的下标，失败则返回-1
 * 
 * @param btmp 指向bitmap的指针
 * @param cnt 连续的位数
 * @return int 起始位的下标
 */
int bitmap_scan(bitmap_t *btmp, uint32_t cnt){
    uint32_t idx_byte = 0;

    // 先逐字节比较
    while ((0xFF == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_byte_len))
        idx_byte++;
    
    ASSERT(idx_byte < btmp->btmp_byte_len);
    // 内存池中无可用空间
    if (idx_byte == btmp->btmp_byte_len)
        return -1;
    
    // 字节内逐位比较
    int idx_bit = 0;
    // 找到空闲位
    while ((uint8_t) (BITMAP_MASK << idx_bit) & btmp->bits[idx_byte])
        idx_bit++;

    int bit_idx_start = idx_byte * 8 + idx_bit;
    // 若只要分配1位，则直接返回
    if (cnt == 1)
        return bit_idx_start;

    // 否则从当前位开始，逐位向后寻找
    uint32_t bit_left = (btmp->btmp_byte_len * 8 - bit_idx_start);
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;

    bit_idx_start = -1;
    while (bit_left-- > 0){
        if (!(bitmap_scan_test(btmp, next_bit)))
            count++;                // next_bit为0，则又找到一个空闲位
        else
            count = 0;
        
        if (count == cnt){
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }

    return bit_idx_start;
}


/**
 * @brief 将bitmap中的bit_idx位设置为value
 * 
 * @param btmp 指向bitmap的指针
 * @param bit_idx 要设置的位的索引
 * @param value 要设置的值
 */
void bitmap_set(bitmap_t *btmp, uint32_t bit_idx, int8_t value){
    ASSERT((value == 1) || (value == 0));

    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd = bit_idx % 8;

    if (value)
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);           // 设置1位
    else
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);          // 设置0位
}