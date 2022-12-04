#ifndef _KERNEL_MEMORY_H
#define _KERNEL_MEMORY_h

#include "stdint.h"
#include "bitmap.h"


#define PG_P_1  1       // 页表项/页目录项的存在位
#define PG_P_0  0       // 页表项/页目录项的存在位
#define PG_RW_R 0       // 页表项R/W位，读/执行权限
#define PG_RW_W 2       // 页表项R/W位，读/写/执行权限
#define PG_US_S 0       // 页表项U/S位，系统级
#define PG_US_U 4       // 页表项U/S位，用户级


#define PDE_IDX(addr)   ((addr & 0xFFC00000) >> 22)     // 宏函数获取页目录偏移
#define PTE_IDX(addr)   ((addr & 0x003FF000) >> 12)     // 宏函数获取页目录偏移


// 虚拟内存(地址)池，用于虚拟地址管理，loader中建立了页目录，但是只分配了一个页(为了操作系统)
typedef struct __virtual_addr_t {
    bitmap_t vaddr_bitmap;
    uint32_t vaddr_start;
} virtual_add_t;

typedef struct __pool_t {
    bitmap_t pool_bitmap;                       // 本内存池的位图，用于管理物理内存
    uint32_t phy_addr_satrt;                    // 本内存池所管理的物理内存的起始地址
    uint32_t pool_size;                         // 本内存池的字节容量
} pool_t;

typedef enum __pool_flags {
    PF_KERNEL = 1,                              // 内核内存池
    PF_USER = 2                                 // 用户内存池
} pool_flags_t;

// kernel_pool和user_pool是物理内存池，并且由于是共享数据，因此实际上对其的操作要保证原子性
extern pool_t kernel_pool, user_pool;

void mem_init(void);
uint32_t* pde_addr(uint32_t vaddr);
uint32_t* pte_addr(uint32_t vaddr);
void *malloc_page(pool_flags_t pf, uint32_t pg_cnt);
void *get_kernel_pages(uint32_t pg_cnt);

#endif