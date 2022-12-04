#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "debug.h"
#include "string.h"
#include "sync.h"
// memory是系统的内存管理模块，因此需要先规划系统的物理内存

// 内核运行时需要1G的物理内存，剩下3G物理内存是用户程序，由于有内存分页，因此物理内存中不必连续，虚拟内存中连续即可
// 用户进程运行在虚拟地址的0~3GB，内核运行在虚拟地址的3~4GB (0xC0000000~0xFFFFFFFF)

// 物理内存的布局如下:
// 0 ~ 1MB                      : 这1MB内存储存了操作系统的代码                 0x00000000~0x00100000
// 1MB ~ 1MB+4K                 : 这4KB的内存存储了页目录                      0x00100000~0x00100FFF
// 1MB+4KB ~ 1MB+8KB            : 这4KB的内存储存了第一个页表，该页表指向0~4MB    0x00101000~0x00101FFF
// 1MB+8KB ~ 1MB+8KB+254*4kb    : 这254*4KB的内存将储存内核运行时候1G的页表      0x00102000~0x00200000
// 2MB以后                       : 都是自由空间

// 页目录的布局如下:
// 第   0个页目录项       -->       0x10_1000处的页表       -->     0x0000_0000 ~ 0x0040_0000:      前4M物理内存
// 第....个页目录项       -->       未分配
// 第....个页目录项       -->       未分配
// 第....个页目录项       -->       未分配
// 第 768个页目录项       -->       0x10_1000处的页表       -->     0x0000_0000 ~ 0x0040_0000:      前4M物理内存
// 第 769个页目录项       -->       0x10_2000处的页表       -->     未分配
// 第 770个页目录项       -->       0x10_3000处的页表       -->     未分配
// 第 771个页目录项       -->       0x10_4000处的页表       -->     未分配
// 第....个页目录项       -->       0x1x_x000处的页表       -->     未分配
// 第....个页目录项       -->       0x1x_x000处的页表       -->     未分配
// 第....个页目录项       -->       0x1x_x000处的页表       -->     未分配
// 第1024个页目录项       -->       0x10_0000处的页表       -->     访问该页0~1023个项没用，因为只是页目录项被解释为了页表项，
//                                                              只有最后一个页目录项才有用，因为这个指向的是页表本身页
// PS: 一个页4096字节，而一个页目录项4个字节，因此页目录能够存放1024个页目录项：

// 虚拟内存的布局如下:
//            虚拟内存地址                                                       物理内存地址                                      物理内存大小
//   用户      0x0000_0000 ~ 0x003F_FFFF        0   ~   4MB             -->     0x0000_0000 ~ 0x0010_0000                       0   ~   4MB
//   用户      0x0040_0000 ~ 0xBFFF_FFFF        4MB ~   3GB             -->     用户数据物理存储区域                                4MB ~   3GB                由于页目录指向的页表中页表项未创建，所以没有具体对应
//   系统      0xC000_0000 ~ 0xC03F_FFFF        3GB ~   3GB + 4MB       -->     0x0000_0000 ~ 0x0010_0000                       0   ~   4MB
//   系统      0xC040_0000 ~ 0xFFBF_FFFF  3GB + 4MB ~   4GB - 4MB       -->     内核数据物理存储区域                                0   ~   4MB
//   系统      0xFFC0_0000 ~ 0xFFFF_EFFF  4GB - 4MB ~   4GB - 4KB       -->     指向各个页表的物理页                                4092KB                     由于页表还没有分配，所以不知道对应关系
//   系统      0xFFFF_F000 ~ 0xFFFF_FFFC  4GB - 4KB ~   4GB - 4B        -->     0x0010_0000 ~ 0x0010_0FFF: 指向各个页目录项        1MB ~   1MB + 4KB - 4B     页目录表中各个页目录项
//   系统      0xFFFF_FFFC ~ 0xFFFF_FFFF  4GB - 4B  ~   4GB             -->     0x0010_0000 ~ 0x0010_0004: 指向页目录表           1MB ~   1MB + 4B


// 程序都有主线程，内核也有。内核的主线程就是main.c中的main。
// 而未来要用PCB/TCB来管理进程（包括用户进程和内核进程），所以内核的主进程也要有一个PCB
// 实现的PCB要占用一整个物理页，从0xXXXXX000~0xXXXXXFFF
// PCB占用的一个页，其最低处0xXXXXX000以上存储的是进程或线程的信息，这包括PID、进程状态等
// 而这个页剩下的空间，（一直到页的最高处OxXXXXXfff）用于进程或线程在0特权级下所使用的栈

// 内核代码在0xC0001500处，内核栈的栈底在0xC009F00处（loader.S跳转到内核前设置），将内核的内存位图放在0xC009A000处
// 所以内核主线程栈顶是0xC009F000，则内核主线程的PCB将在0xC009E000，内核堆将在0xC0100000处

// 虚拟地址0xC0000000~0xC00FFFFF被映射到了物理地址0x00000000~0x000FFFFF处
// 所以堆从页目录第二项开始，即内核堆从0xC0100000处开始
// 但是注意，物理内存0x00100000~0x00100FFF存放了页目录,0x00101000~0x00101FFF存放了第一个页表
// 因此虚拟地址0xC0100000~0xC0101FFF要重新映射

// 一个物理页大小的位图可以表示的内存4096 byte * 8 bit * 4 KB = 128M，完全足够内核使用了，所以使用一个页来管理内核的内存就可以了
#define MEM_BITMAP_BASE 0xC009A000      // 内核的位图放在0xC009A000处
#define K_HEAP_START 0xC0100000         // 内核的堆建立在0xC0100000处, 只是选择在这里, 这里以及后面一大片区域，全是内核的内存


/* ================================================================================================================== */
/* ================================================= 内存初始化函数 =================================================== */
/* ================================================================================================================== */


typedef struct __pool_t {
    bitmap_t pool_bitmap;                       // 本内存池的位图，用于管理物理内存
    uint32_t phy_addr_start;                    // 本内存池所管理的物理内存的起始地址
    uint32_t pool_size;                         // 本内存池的字节容量
    mutex_t mutex;                              // 内存池是共享变量，申请内存时候要保证互斥
} pool_t;



pool_t kernel_pool, user_pool;          // 内核内存池和用户内存池
virtual_addr_t kernel_vaddr;             // 用于管理内核虚拟地址


/**
 * @brief mem_pool_init用于初始化内存池
 * 
 * @param all_mem 当前系统的内存数，以字节为单位
 */
static void mem_pool_init(uint32_t all_mem){
    put_str("    mem_pool_init start\n");


    // 首先需要计算可用的空间。
    // 目前已经占用的空间 = 1MB系统内核 + 页目录占用的页 + 第一个页表占用的页 + 操作系统1G的运行空间页表
    //      0x0~0x100000这1MB给了内核
    //      0x100000~0x100FFF第一个物理页存放了页目录
    //      0x101000~0x101FFF第二个物理页存放了第一个页表（该页表指向0~4MB空间，即内核）
    //      操作系统运行占用1G内存，描述这部分内存用了是页目录中C00~FFE这254个页表，而一个
    //      页表要用一个物理页保存，所以描述操作系统运行所需要的内存需要254物理页以保存这254个页表（虽然这254个页目前只初始化了第一个页）
    // 所以目前已经已经用了：1MB系统内核 + 256个物理页 * 4KB
    uint32_t page_table_size = PG_SIZE * 256;
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;
    uint16_t all_free_page = free_mem / PG_SIZE;

    // 剩下的物理页就将用为操作系统和用户进程的页，用于malloc时候分配，为了简单起见，系统和用户对半分，但系统肯定用不完
    uint16_t kernel_free_pages = all_free_page / 2;
    uint16_t user_free_pages = all_free_page - kernel_free_pages;

    // 初始化内核物理内存池，内存池用bitmap来描述
    uint32_t kbm_length = kernel_free_pages / 8;                        // 描述内核物理内存的bitmap (kernel bitmap)的长度
    uint32_t kp_start = used_mem;                                       // 内核内存池从1MB + 1MB后开始
    kernel_pool.phy_addr_start = kp_start;                              // 设置内核物理内存开始地址为已经使用的内存之后
    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    kernel_pool.pool_bitmap.btmp_byte_len = kbm_length;
    kernel_pool.pool_bitmap.bits = (void *) MEM_BITMAP_BASE;            // 
    bitmap_init(&kernel_pool.pool_bitmap);                              // 位图清0

    // 初始化用户物理内存池
    uint32_t ubm_length = user_free_pages / 8;
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;
    user_pool.phy_addr_start = up_start;
    user_pool.pool_size = user_free_pages * PG_SIZE;
    user_pool.pool_bitmap.btmp_byte_len = ubm_length;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);
    bitmap_init(&user_pool.pool_bitmap);                                // 位图清0

    // print info 
    put_str("    kernel_pool.bitmap_start: ");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool.phy_addr_start: ");
    put_int((int)kernel_pool.phy_addr_start);
    put_char('\n');

    put_str("    user_pool.bitmap_start: ");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool.phy_addr_start: ");
    put_int((int)user_pool.phy_addr_start);
    put_char('\n');


    mutex_init(&user_pool.mutex);
    mutex_init(&kernel_pool.mutex);
    // 内核虚拟内存初始化
    // 内核虚拟地址位图按照物理内存大小初始化
    kernel_vaddr.vaddr_bitmap.btmp_byte_len = kbm_length;
    // 内核虚拟地址位图也要一块内存储存，目前定位置在内核物理内存位图和用户物理内存位图后面
    kernel_vaddr.vaddr_bitmap.bits = (void*) (MEM_BITMAP_BASE + kbm_length +ubm_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;                            // 内核虚拟内存的起始地址为
    bitmap_init(&kernel_vaddr.vaddr_bitmap);

    put_str("    mem_pool_init done\n");
}


/**
 * @brief mem_init用于初始化内存
 */
void mem_init(){
    put_str("mem_init start\n");
    uint32_t mem_byte_total = (*(uint32_t*) (0xb00));           // loader.S中获取了系统当前的内存，保存在0xb00中，现在获取该值
    mem_pool_init(mem_byte_total);
    put_str("mem_init done\n");
}


/* ================================================================================================================== */
/* ================================================= 通用内存分配函数 ================================================== */
/* ================================================================================================================== */


// 申请一个页的流程：
//      1. 首先需要在虚拟内存池中申请得到一个虚拟页
//      2. 然后需要在物理内存池中申请得到一个物理页
//      3. 最后在页表中完成虚拟页和物理页的映射, 即完成虚拟地址转物理地址
// 举个例子：
//      1. 首先申请虚拟内存中的一个页，假设目前可用的就是0x1000_1000~0x1000_1FFF这个页
//      2. 然后在物理内存池中申请一个页，假设可用的物理地址就是0xD0AF_C000~0xD0AF_CFFF这个页
//      3. 0x1000_1000虚拟地址拆出来就是：0b0001_0000_00:0b00_0000_0001:0x000
//          3.1 首先在页目录表中查询040偏移对应的页表的物理地址，假设页表物理地址是0x12345000
//          3.2 然后在页表物理地址中将001对应的页的物理地址s设置为0xD0AF_C000
//      但是存在问题：
//          1. 页目录项可能不存在，此时需要新建一个页表

/**
 * @brief vaddr_get用于在指定的虚拟内存池中申请pg_cnt个虚拟页
 * 
 * @param pf 申请内存的内存池
 * @param pg_cnt 申请的虚拟页个数
 * @return void* 若申请成功则返回虚拟页的起始地址，失败则返回NULL
 */
static void* vaddr_get(pool_flags_t pf, uint32_t pg_cnt){
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL){
        // 从内核虚拟内存池中分配页
        // 扫描虚拟内存池中的bitmap, 查看是否存在pg_cnt个连续的页
        if ((bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt)) == -1)
            return NULL;
        // 设置位图中的位
        while (cnt < pg_cnt)
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        // 从用户虚拟内存池中分配页
        task_struct_t *cur = running_thread();
        if ((bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt)) == -1)
            return NULL;
        // 设置位图中的位
        while (cnt < pg_cnt)
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

        // 0xC000_0000 ~ 0xC000_0FFF将用于存储用户的三级栈，因此分配得到的空间覆盖这里
        ASSERT((uint32_t) vaddr_start < (0xC0000000 - PG_SIZE));
    }
    return (void *) vaddr_start;
}


/**
 * @brief palloc用于在m_pool指向的内存池中分配1个物理页
 * 
 * @param m_pool 要分配物理页的内存池的地址
 * @return void* 若成功，则返回物理页第一个字节的物理地址，若失败则返回NULL
 */
static void* palloc(pool_t* m_pool){
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
    if (bit_idx == -1)
        return NULL;
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*) page_phyaddr;
}


/**
 * @brief page_table_add用于在页表中添加虚拟地址所属的虚拟页与物理地址所属的物理页的映射。
 *        注意，给出虚拟地址和物理地址即可，会自动计算需要映射的虚拟页和物理页会被
 * 
 * @param _vaddr 被映射的虚拟地址
 * @param _page_phyaddr 要映射到的物理地址
 * 
 */
static void page_table_add(void* _vaddr, void* _page_phyaddr){
    uint32_t vaddr = (uint32_t) _vaddr;
    uint32_t page_phyaddr = (uint32_t) _page_phyaddr;

    // 获取页表地址和页地址，这两个包含在pde和pte中
    // Attention 等下手动查一下页表
    uint32_t* pt_addr = pde_addr(vaddr);
    uint32_t* p_addr = pte_addr(vaddr);

    // 直接执行*p_add可能会报错，因为*p_addr所在的页表pt_addr可能为空，所以必须要确保在页表创建完成后再执行*p_addr，否则会引起page_fault
    // 首先确定虚拟地址的*pt_addr存在
    if (*pt_addr & 0x00000001){
        ASSERT(!(*p_addr & 0x00000001));            // 确保当前pte没有被使用，即
        if (!(*p_addr & 0x00000001))
            *p_addr = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        else{
            PANIC("pte repeat");
        }
    } else {
        // pt_addr不存在，需要首先进行创建页目录项
        uint32_t pt_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pt_addr = (pt_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        // 页表中的数据要清0，避免原有的数据被误认为是页表项
        memset((void *) ((int)p_addr & 0xFFFFF000), 0, PG_SIZE);
        ASSERT(!(*p_addr & 0x00000001));
        *p_addr = (page_phyaddr | PG_US_U | PG_RW_W |PG_P_1);
    }
}


/**
 * @brief get_a_page用于从指定的内存池中分配一个页，并将该页与虚拟地址vaddr所属的虚拟页绑定
 * 
 * @param pf 分配页的内存池
 * @param vaddr 需要绑定的地址
 * @return void* 被绑定后的地址，等于(void*) vaddr
 */
void *get_a_page(pool_flags_t pf, uint32_t vaddr){
    pool_t *mem_pool = pf & PF_KERNEL ?  &kernel_pool : &user_pool;
    mutex_acquire(&mem_pool->mutex);

    task_struct_t *cur = running_thread();

    // 先将虚拟内存位图中对应的位图设置为1
    int32_t bit_idx = -1;
    if (cur->pgdir != NULL && pf == PF_USER){
        // 若是用户进程申请用户内存，并且用户进程已经有虚拟内存页，则修改用户进程虚拟内存位图
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
    } else if (cur->pgdir == NULL && pf == PF_KERNEL) {
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    } else 
        PANIC("get_a_page: kernel allocates usersapce or user allocate kernelspace is not allowed!");


    // 分配一个物理页
    void *page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL)
        return NULL;
    
    // 页表中添加虚拟页和物理页的映射
    page_table_add((void*)vaddr, page_phyaddr);
    
    // 释放锁
    mutex_release(&mem_pool->mutex);

    return (void*) vaddr;
}


/**
 * @brief addr_v2p用于将虚拟地址转为物理地址
 * 
 * @param vaddr 需要转换的虚拟地址
 * @return uint32_t 虚拟地址对应的物理地址
 */
uint32_t addr_v2p(uint32_t vaddr){
    uint32_t *page_addr = pte_addr(vaddr);
    // 去掉页表项低12位的页表项属性，而后拼接虚拟地址的低12位页内偏移得到物理地址
    return ((*page_addr & 0xFFFFF000) + (vaddr & 0x00000FFF));
}


/* ================================================================================================================== */
/* ================================================== 内核分配一个页 ================================================== */
/* ================================================================================================================== */

/**
 * @brief pte_addr用于获得指向给定虚拟地址所在的物理页的地址，即pte中的物理地址
 * 
 * @param vaddr 需要获得pde地址的虚拟地址
 * @return uint32_t* 虚拟地址的pte地址
 */
uint32_t* pte_addr(uint32_t vaddr){
    // 先访问页目录本身，然后访问页目录的偏移，得到PTE的地址
    uint32_t *pte = (uint32_t*) (0xFFC00000 + ((vaddr & 0xFFC00000) >> 10) + PTE_IDX(vaddr) * 4);
    return pte;
}


/**
 * @brief pde_addr用于获得指向给定虚拟地址所在的页表的地址，即pde中的物理地址
 * 
 * @param vaddr 需要获得pde地址的虚拟地址
 * @return uint32_t* 虚拟地址的pte地址
 */
uint32_t* pde_addr(uint32_t vaddr){
    uint32_t *pde = (uint32_t*) ((0xFFFFF000) + PDE_IDX(vaddr) * 4);
    return pde;
}




/**
 * @brief malloc_page从pf指定的内存池中分配pg_cnt个页
 * 
 * @param pf 指定要分配的内存池
 * @param pg_cnt 要分配的页
 * @return void* 若分配成功，则返回虚拟地址，失败则返回NULL
 */
void *malloc_page(pool_flags_t pf, uint32_t pg_cnt){
    ASSERT(pg_cnt > 0 && pg_cnt < 3840);
    // malloc_page的流程：
    //      1. 首先需要在虚拟内存池中申请得到一个虚拟页
    //      2. 然后需要在物理内存池中申请得到一个物理页
    //      3. 最后在页表中完成虚拟页和物理页的映射, 即完成虚拟地址转物理地址

    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL)
        return NULL;

    uint32_t vaddr = (uint32_t) vaddr_start, cnt = pg_cnt;
    pool_t* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    // 对每个物理页进行映射
    while (cnt-- > 0){
        void *page_phyaddr = palloc(mem_pool);
        // 如果申请物理页失败，已经获得虚拟页要归还，后面再实现，也可以实现换页
        if (page_phyaddr == NULL)
            return NULL;
        // 在二级页表中插入页表项
        page_table_add((void*) vaddr, page_phyaddr);
        // 下一个虚拟页
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}


/**
 * @brief get_kernel_page用于从内核内存池中申请pg_cnt个页
 * 
 * @param pg_cnt 要申请的页的数量
 * @return void* 申请成功则返回得到的虚拟地址，若失败则返回NULL
 */
void *get_kernel_pages(uint32_t pg_cnt){
    mutex_acquire(&kernel_pool.mutex);
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL)
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    mutex_release(&kernel_pool.mutex);
    return vaddr;
}




/* ================================================================================================================== */
/* ================================================= 用户进程分配一个页 ================================================ */
/* ================================================================================================================== */


/**
 * @brief get_user_page用于从用户内存池中申请pg_cnt个页
 * 
 * @param pg_cnt 要申请的页的数量
 * @return void* 申请成功则返回得到的虚拟地址，若失败则返回NULL
 */
void* get_user_pages(uint32_t pg_cnt){
    // 未来可能会有多个用户进程，因此需要上锁
    mutex_acquire(&user_pool.mutex);
    void *vaddr = malloc_page(PF_USER, pg_cnt);
    // if (vaddr != NULL)
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    mutex_release(&user_pool.mutex);
    return vaddr;
}

