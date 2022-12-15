#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdint.h"
#include "bitmap.h"
#include "list.h"


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
    bitmap_t vaddr_bitmap;                      // 虚拟内存的位图
    uint32_t vaddr_start;                       // 虚拟内存的起始地址
} virtual_addr_t;

typedef enum __pool_flags {
    PF_KERNEL = 1,                              // 内核内存池
    PF_USER = 2                                 // 用户内存池
} pool_flags_t;

// kernel_pool和user_pool是物理内存池，并且由于是共享数据，因此实际上对其的操作要保证原子性
extern struct __pool_t kernel_pool, user_pool;

/**
 * @brief mem_init用于初始化系统的内存
 * 
 * @details mem_init干的事:
 *              1. 初始化页粒度内存管理系统:
 *                  1.1 初始化内核使用的物理内存Bitmap
 *                  1.2 初始化用户使用的物理内存Bitmap
 *                  1.3 初始化内核使用的虚拟内存Bitmap
 *              2. 初始化细粒度内存管理系统
 */
void mem_init(void);

/**
 * @brief pde_addr用于获得指向给定虚拟地址所在的页表的地址，即pde中的物理地址
 * 
 * @param vaddr 需要获得pde地址的虚拟地址
 * @return uint32_t* 虚拟地址的pte地址
 */
uint32_t* pde_addr(uint32_t vaddr);

/**
 * @brief pte_addr用于获得指向给定虚拟地址所在的物理页的地址，即pte中的物理地址
 * 
 * @param vaddr 需要获得pde地址的虚拟地址
 * @return uint32_t* 虚拟地址的pte地址
 */
uint32_t* pte_addr(uint32_t vaddr);


/**
 * @brief malloc_page从pf指定的内存池中分配pg_cnt个页
 * 
 * @param pf 指定要分配的内存池
 * @param pg_cnt 要分配的页
 * @return void* 若分配成功，则返回虚拟地址，失败则返回NULL
 */
void *malloc_page(pool_flags_t pf, uint32_t pg_cnt);


/**
 * @brief get_kernel_page用于从内核内存池中申请pg_cnt个页
 * 
 * @param pg_cnt 要申请的页的数量
 * @return void* 申请成功则返回得到的虚拟地址，若失败则返回NULL
 */
void *get_kernel_pages(uint32_t pg_cnt);


/**
 * @brief get_user_page用于从用户内存池中申请pg_cnt个页
 * 
 * @param pg_cnt 要申请的页的数量
 * @return void* 申请成功则返回得到的虚拟地址，若失败则返回NULL
 */
void *get_user_pages(uint32_t pg_cnt);


/**
 * @brief get_a_page用于从指定的内存池中分配一个页，并将该页与虚拟地址vaddr所属的虚拟页绑定
 * 
 * @param pf 分配页的内存池
 * @param vaddr 需要绑定的地址
 * @return void* 被绑定后的地址，等于(void*) vaddr
 */
void *get_a_page(pool_flags_t pf, uint32_t vaddr);


/**
 * @brief mfree_page将释放以虚拟地址vaddr所在的物理页为起始的pg_cnt个物理页
 * 
 * @param pf 要释放的内存池
 * @param _vaddr 要释放的
 * @param pg_cnt 要释放的物理页的数量
 */
void mfree_page(pool_flags_t pf, void *_vaddr, uint32_t pg_cnt);


/**
 * @brief addr_v2p用于将虚拟地址转为物理地址
 * 
 * @param vaddr 需要转换的虚拟地址
 * @return uint32_t 虚拟地址对应的物理地址
 */
uint32_t addr_v2p(uint32_t vaddr);



/**
 * @brief 内存块单元, 本质是链表中的, 将被连接在mem_block_desc_t.free_list中
 * 
 */
typedef struct __mem_block_t {
    list_elem_t free_elem;
} mem_block_t;

/**
 * @brief 内存块描述符, 或者说内存块售货窗口, 本质就是一个链表. 
 *          目前一共有7种不同size的内存描述符, 分别是: 16字节, 32字节, 64字节, 128字节, 256字节, 512字节, 1024字节
 *          在申请内存的时候, 根据申请的内存块大小不同的, 分别从不同block_size的mem_block_desc中去获取内存
 * 
 * @details Memory view of mem_block_desc_t, Arena and mem_block. Page is a free virtual memory page.
 * 
 *    mem_block_desc_t  ----------------
 *                      |  block_size  |
 *                      ----------------
 *                      |  blocks...   |
 *                      ----------------
 *        ,------------ |  free_list   |
 *        |             ----------------
 *        |                             ^
 *        |                              \
 *        |   Page                        \                 Offset:
 *        |   ------------------------------\-------                Page Start
 *        |   |   Arena   ------------------ |     |
 *        |   |           |  Arena.desc    |-'     |                0x000
 *        |   |           ------------------       |
 *        |   |           |  Arena.free_cnt|       |                0x003
 *        |   |           ------------------       |
 *        |   |           |  Arena.large   |       |                0x007
 *        |   |           ------------------       |
 *        |   |------------------------------------|
 *        |   |        -------------------------   |
 *        '--------->  |  mem_block.list_elem  | ------,            0x00B
 *            |        |-----------------------|   |   |
 *            |        |          free         |   |   |              The small chunk of memory consists of mem_block.list_elem and free space to use 
 *            |        |          space        |   |   |               is what user will actually use after malloc, i.e. (void*)malloc(xxx) will point
 *            |        |          to           |   |   |               to the first byte of `free space to use`. 
 *            |        |          use          |   |   |              Besides, a few bytes taken by mem_block.list_elem can be overwrite by user program, 
 *            |        -------------------------   |   |               since free will rebuild a mem_block.list_elem on the head of the memory chunk.
 *            |                                    |   |               
 *            |        -------------------------   |   |
 *        ,----------  |  mem_block.list_elem  | <-----'            0x00B + sizeof(free sapce to use)
 *        |   |        |-----------------------|   |
 *        |   |        |          free         |   |
 *        |   |        |          space        |   |
 *        |   |        |          to           |   |
 *        |   |        |          use          |   |
 *        |   |        -------------------------   |
 *        V   |                                    |
 *        .   |                   .                |                .
 *        .   |                   .                |                .
 *        .   |                   .                |                .
 *            |                                    |
 *            --------------------------------------                Page End
 */
typedef struct __mem_block_desc_t{
    /// @brief 当前内存售货窗口中每个内存单元的大小
    uint32_t block_size;
    /// @brief 当前内存售货窗口中每箱水中的瓶子数
    uint32_t blocks_per_arena;
    /// @brief 连接所有block的链表, 每个元素都memory block
    list_t free_list;
} mem_block_desc_t;

/// @brief 内存块描述符个数, 一共有7种不同size的内存描述符
#define MEM_UNIT_CNT 7


/**
 * @brief block_desc_init用于初始化mem_block_desc_t
 * @param desc_array 指向将要初始化的mem_block_desc_t的指针
 */
void block_desc_init(mem_block_desc_t *desc_array);


/**
 * @brief pfree(Physical Free)用于将给定的物理地址所属于的页回收到物理内存池
 * 
 * @details 由于使用Bitmap的方式管理物理内存, 而Bitmap中一位表示4K大小的内存, 即一个页.
 *          而回收本质上就是把对应页在bitmap中的占用位的清0即可. 这样做一方面减小了开销, 
 *          但是却造成了内存泄露的问题, 所以在用户申请一个页的时候, 需要memset清0
 * 
 * @param pg_phy_addr 
 */
void pfree(uint32_t pg_phy_addr);

/**
 * @brief sys_malloc是malloc系统调用的实现函数, 用于在当前进程的堆中申请size个字节的内存
 * 
 * @details 开启虚拟内存以后, 只有真正的分配物理页, 在页表中添加物理页和虚拟页的映射才会接触到物理页,
 *          除此以外所有分配内存, 分配的都是虚拟内存
 * 
 * @param size 要申请的内存字节数
 * @return void* 若分配成功, 则返回申请得到的内存的首地址; 失败则返回NULL
 */
void* sys_malloc(uint32_t size);


/**
 * @brief sys_free用于释放sys_malloc分配的内存
 * 
 * @param ptr 指向由sys_malloc分配的物理内存
 */
void sys_free(void* ptr);

#endif