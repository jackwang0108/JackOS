#include "pipe.h"
#include "file.h"
#include "fork.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "process.h"
#include "interrupt.h"

extern void intr_exit(void);

/**
 * @brief copy_pcb_vaddrbitmap_stack0用于将父线程的tcb信息复制到子线程的tcb中. 同时
 *        会为子线程单独拷贝一份虚拟内存的bitmap.
 * 
 * @param child_thread 子进程pcb
 * @param parent_thread 父进程pcb
 * @return uint32_t 复制成功则返回0
 */
static int32_t copy_pcb_vaddrbitmap_stack0(task_struct_t *child_thread, task_struct_t *parent_thread){
    // 复制整个页, 包括内核线程的tcb和内核栈
    memcpy(child_thread, parent_thread, PG_SIZE);

    // 单独修改子进程信息
    child_thread->pid = fork_pid();
    child_thread->total_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->this_tick = child_thread->time_slice;
    child_thread->parent_pid = parent_thread->pid;
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
    block_desc_init(child_thread->u_block_desc);

    // 复制父进程虚拟地址池的位图, 因为每个进程的虚拟内存都是独立的, 所以需要单独复制
    uint32_t bitmap_pg_cnt = DIV_CEILING((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    void *vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    if (vaddr_btmp == NULL)
        return -1;

    // 因为上面直接复制的整个页, 所以child_thread->userprog_vaddr依旧指向父进程的bitmap
    // 这里逐字节的复制, 然后再转换下类型
    memcpy(vaddr_btmp, child_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PG_SIZE);
    child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;

    // prepare for name
    // ASSERT(strlen(child_thread->name) < 11);
    // strcat(child_thread->name, "_fork");
    return 0;
}


/**
 * @brief copy_body_stack3用于拷贝父进程的在内存中的所有数据给子进程, 即复制一份进程实体.
 *        该函数将被父进程调用, 此时使用的页目录表是父进程的. 
 * 
 * @param child_thread 被复制的子进程
 * @param parent_thread 被复制的父进程
 * @param buf_page 内核页, 用于缓冲用
 */
static void copy_body_stack3(task_struct_t *child_thread, task_struct_t*parent_thread, void *buf_page){
    uint8_t *vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_byte_len;

    uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;
    uint32_t idx_byte = 0;
    uint32_t idx_bit = 0;
    uint32_t prog_vaddr = 0;

    // 遍历父进程的所有的虚拟页
    while (idx_byte < btmp_bytes_len){
        if (vaddr_btmp[idx_byte]){
            idx_bit = 0;
            while (idx_bit < 8){
                // 若父进程中的该虚拟页正在使用
                if ((BITMAP_MASK << idx_bit) & vaddr_btmp[idx_byte]){
                    // 计算当前页虚拟地址
                    prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;
                    // 将父进程该虚拟页复制到buf_page中
                    // buf_page必须是内核页, 因为不同进程的内核空间是共享的, 所以才能实现切换
                    // cr3寄存器, 但是buf_page中的内容依旧不变
                    memcpy(buf_page, (void*)prog_vaddr, PG_SIZE);
                    // 使用子进程的页目录, 此后操作的就是子进程的虚拟内存
                    page_dir_activate(child_thread);
                    // 从用户物理内存池中申请一个页, 并映射该页到子进程的也目录表中
                    get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);
                    // 复制buf_page中的内容到子进程的页中
                    memcpy((void*)prog_vaddr, buf_page, PG_SIZE);
                    // 重新切换为父进程的虚拟页表
                    page_dir_activate(parent_thread);
                }
                idx_bit++;
            }
        }
        idx_byte++;
    }
}


/**
 * @brief 为子进程构建thread_stack. 之所以要构建thread_stack是因为该函数作为fork系统调用一部分
 *        必然是用户调用系统调用, 则一定是发生了0x80软中断. 所以在返回的时候必然是经过intr_exit的
 *        所以这个时候就会用到thread_stack
 * 
 * @param child_thread 
 * @return int32_t 
 */
static int32_t build_child_stack(task_struct_t *child_thread){
    // 获取用户进程的0级栈
    intr_stack_t *intr_0_stack = (intr_stack_t *) ((uint32_t) child_thread + PG_SIZE - sizeof(intr_stack_t));
    intr_0_stack->eax = 0;

    // 为switch_to构建tread_stack
    uint32_t *ret_addr_in_thread_stack = (uint32_t *)intr_0_stack - 1;
    uint32_t *esi_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 2;
    uint32_t *edi_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 3;
    uint32_t *ebx_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 4;
    // ebp中的内容是0级栈esp的内容
    uint32_t * ebp_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 5;

    // 填充thread_stack
    *ret_addr_in_thread_stack = (uint32_t) intr_exit;           // switch_to的返回地址设置为intr_exit
    *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;

    // 将thread_stack的栈顶作为switch_to恢复数据时的栈顶
    child_thread->self_kstack = ebp_ptr_in_thread_stack;
    return 0;
}


/**
 * @brief update_inode_open_cnts用于更新tcb中的fd_table中所有打开文件的引用数
 * 
 * @param tcb 需要更新的tcb
 */
static void update_inode_open_cnts(task_struct_t *tcb){
    int32_t local_fd = 3, global_fd = 0;
    while (local_fd < MAX_FILE_OPEN_PER_PROC){
        global_fd = tcb->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        if (global_fd != -1){
            if (is_pipe(local_fd))
                file_table[global_fd].fd_pos++;
            else 
                file_table[global_fd].fd_inode->i_open_cnt++;
        }
        local_fd++;
    }
}


/**
 * @brief copy_process用于将父进程中的信息复制给子进程
 * 
 * @param child_thread 子进程
 * @param parent_thread 被复制的父进程
 * @return uint32_t 复制成功返回0; 复制失败返回-1
 */
static int32_t copy_process(task_struct_t *child_thread, task_struct_t *parent_thread){
    // 分配内核缓冲页
    void *buf_page = get_kernel_pages(1);
    if (buf_page == NULL)
        return -1;

    // 复制父进程的pcb, 虚拟地址位图, 内核栈给子进程
    if (copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1)
        return -1;
    
    // 为子进程创建页表
    child_thread->pgdir = create_page_dir();
    if (child_thread->pgdir == NULL)
        return -1;

    // 复制父进程的所有数据给子进程
    copy_body_stack3(child_thread, parent_thread, buf_page);

    // 构建子进程thread_stack并且修改返回值
    build_child_stack(child_thread);

    // 更新文件计数
    update_inode_open_cnts(child_thread);

    mfree_page(PF_KERNEL, buf_page, 1);
    return 0;
}


/**
 * @brief sys_fork是fork系统调用的实现函数. 用于复制出来一个子进程. 子进程的数据和代码和父进程的数据代码一模一样
 * 
 * @return pid_t 父进程返回子进程的pid; 子进程返回0
 */
pid_t sys_fork(void){
    task_struct_t *parent_thread = running_thread();

    task_struct_t *child_thread = get_kernel_pages(1);
    if (child_thread == NULL)
        return -1;

    // 必须关中断, 而且父进程一定是进程
    ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);

    // 复制所有数据
    if (copy_process(child_thread, parent_thread) == -1)
        return -1;
    
    // 插入到就绪队列中
    ASSERT(!elem_find(&thread_ready_list, &child_thread->general_tag));
    list_append(&thread_ready_list, &child_thread->general_tag);
    ASSERT(!elem_find(&thread_all_list, &child_thread->all_list_tag));
    list_append(&thread_all_list, &child_thread->all_list_tag);

    return child_thread->pid;
}