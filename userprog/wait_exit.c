#include "fs.h"
#include "file.h"
#include "pipe.h"
#include "debug.h"
#include "thread.h"
#include "wait_exit.h"


/**
 * @brief release_prog_resource用于释放用户进程所占用的特殊资源. 
 *        相比于一般内核线程, 用户进程占用的特殊资源有:
 *              1. 页目录表占用的物理页
 *              2. 虚拟内存池占用的物理页
 *              3. 打开的文件
 *        因此, 在释放的时候也会回收上面这三个特殊的资源
 * 
 * @param tcb 需要回收的线程tcb
 */
static void release_prog_resource(task_struct_t *tcb){
    uint32_t *pgdir_vaddr = tcb->pgdir;
    uint16_t user_pde_num = 768, pde_idx = 0;
    uint32_t pde = 0;
    uint32_t *v_pde_ptr = NULL;

    uint16_t user_pte_num = 1024, pte_idx = 0;
    uint32_t pte = 0;
    uint32_t *v_pte_ptr = NULL;

    uint32_t *first_pte_vaddr_in_page = NULL;
    uint32_t pg_phy_addr = 0;

    // 回收页表中用户空间占用的所有物理页, 用户占用的页目录表项一共768个, 表示4G空间
    while (pde_idx < user_pde_num){
        v_pde_ptr = pgdir_vaddr + pde_idx;
        pde = *v_pde_ptr;
        // 当前页目录项为0, 则页表中所有页表项都没有分配, 直接下一个就行了
        if (pde & 0x00000001){
            // 当前页表项为1, 页表中至少有一个页表项已经分配, 稍后循环检查即可
            first_pte_vaddr_in_page = pte_addr(pde_idx * 0x400000);
            pte_idx = 0;
            // 循环检查, 释放页表项
            while (pte_idx < user_pte_num){
                v_pte_ptr = first_pte_vaddr_in_page + pte_idx;
                pte = *v_pte_ptr;
                // 当前页表项为0, 则该页没有进行映射, 跳过即可
                if (pte & 0x00000001){
                    // 当前页表项为1, 则该页经行了映射, 释放
                    pg_phy_addr = pte & 0xFFFFF000;
                    free_a_phy_page(pg_phy_addr);
                }
                pte_idx++;
            }
            // 释放页表
            pg_phy_addr = pde & 0xFFFFF000;
            free_a_phy_page(pg_phy_addr);
        }
        pde_idx++;
    }

    // 释放虚拟线程池
    uint32_t bitmap_pg_cnt = tcb->userprog_vaddr.vaddr_bitmap.btmp_byte_len / PG_SIZE;
    uint8_t *user_vaddr_pool_bitmap = tcb->userprog_vaddr.vaddr_bitmap.bits;
    mfree_page(PF_KERNEL, user_vaddr_pool_bitmap, bitmap_pg_cnt);

    // 关闭打开的文件
    uint8_t local_fd = 3;
    while (local_fd < MAX_FILE_OPEN_PER_PROC){
        if (tcb->fd_table[local_fd] != -1){
            if (is_pipe(local_fd)){
                uint32_t global_fd = fd_local2global(local_fd);
                if (--file_table[global_fd].fd_pos == 0){
                    mfree_page(PF_KERNEL, file_table[global_fd].fd_inode, 1);
                    file_table[global_fd].fd_inode = NULL;
                }
            } else{
                sys_close(local_fd);
            }
        }
        local_fd++;
    }
}


/**
 * @brief find_child用于查找parent_pid所表示的线程的所有子线程
 * 
 * @param elem 表示线程的tag
 * @param parent_pid 父进程pid
 * @return true elem表示的线程是parent_pid表示的父进程的子线程
 * @return false elem表示的线程不是parent_pid表示的父进程的子线程
 */
static bool find_child(list_elem_t *elem, int32_t parent_pid){
    task_struct_t *tcb = elem2entry(task_struct_t, all_list_tag, elem);
    if (tcb->parent_pid == parent_pid)
        return true;
    return false;
}


/**
 * @brief find_hanging_child用于查找parent_pid表示的父进程的所有状态为TASK_HANGING的子进程
 * 
 * @param elem 表示线程的tag
 * @param parent_pid 父线程pid
 * @return true elem表示的线程是parent_pid表示的父进程的子线程, 并且是TASK_HANGING状态
 * @return false elem表示的线程不是parent_pid表示的父进程的子线程, 并且是TASK_HANGING状态
 */
static bool find_hanging_child(list_elem_t *elem, int32_t parent_pid){
    task_struct_t *tcb = elem2entry(task_struct_t, all_list_tag, elem);
    if (tcb->parent_pid == parent_pid && tcb->status == TASK_HANGING)
        return true;
    return false;
}


/**
 * @brief init_adopt_a_child用于将pid表示的进程过继给init
 * 
 * @param elem 表示线程的tag
 * @param pid 需要过继的线程的pid
 * @return true 当前进程是要过继的进程
 * @return false 当前进程不是要过继的进程
 */
static bool init_adopt_a_child(list_elem_t *elem, int32_t pid){
    task_struct_t *tcb = elem2entry(task_struct_t, all_list_tag, elem);
    if (tcb->parent_pid == pid)
        tcb->parent_pid = 1;
    return false;
}


/**
 * @brief sys_wait是wait系统调用的实现函数. 用于让父进程等待子进程调用exit退出, 并将子进程的返回值保存到status中
 * 
 * @param status 子进程的退出状态
 * @return pid_t 若等待成功, 则返回子进程的pid; 若等待失败, 则返回-1
 */
pid_t sys_wait(int32_t *status){
    task_struct_t *parent_tcb = running_thread();

    while (1){
        // 首先处理所有处理hanging状态的线程, 即父进程调用wait的时候, 子进程已经运行结束
        list_elem_t *child_elem = list_traversal(&thread_all_list, find_hanging_child, parent_tcb->pid);
        // 有挂起的线程, 则进行处理
        if (child_elem != NULL){
            task_struct_t *child_tcb = elem2entry(task_struct_t, all_list_tag, child_elem);
            *status = child_tcb->exit_status;
            uint16_t child_pid = child_tcb->pid;
            // 释放子线程的数据
            thread_exit(child_tcb, false);
            // 返回子线程的pid
            return child_pid;
        }

        // 然后处理所有子线程, 即父进程调用wait的时候, 子进程还没有运行结束
        child_elem = list_traversal(&thread_all_list, find_child, parent_tcb->pid);
        // 父进程没有子进程
        if (child_elem == NULL)
            return -1;
        // 子进程还没有运行结束, 此时阻塞父进程
        thread_block(TASK_WAITING);
    }
}


/**
 * @brief sys_exit是exit系统调用的实现函数. 用于主动结束调用的进程
 */
void sys_exit(int32_t status){
    task_struct_t *child_tcb = running_thread();
    child_tcb->exit_status = status;
    if (child_tcb->parent_pid == -1)
        PANIC("sys_exit: child_tcb->parent_pid is -1\n");
    
    // 把child_thread的所有的子进程都过继给init
    list_traversal(&thread_all_list, init_adopt_a_child, child_tcb->pid);
    // 回收child_thread的资源
    release_prog_resource(child_tcb);
    // 唤醒
    task_struct_t *parent_tcb = pid2thread(child_tcb->parent_pid);
    if (parent_tcb->status == TASK_WAITING)
        thread_unblock(parent_tcb);

    // 将自己挂起, 等待父进程回收PCB
    thread_block(TASK_HANGING);
}