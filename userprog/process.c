#include "process.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "list.h"
#include "tss.h"
#include "interrupt.h"
#include "string.h"
#include "console.h"
#include "print.h"

/**
 * @brief start_process用于构建用户进程初始上下文，原理就是伪装从用户态中断进入内核运行。
 *      具体来说就是在create_thread中为用户进程在该页的页底分配了一个intr_stack_t来伪装中断压栈。
 *      此外，该函数是在当前用户进程已经开始运行时才被调用
 * 
 * @param filename_ 要启动的用户进程的路径
 * 
 * @note 目前由于还没有实现文件系统，因此目前只能运行单纯的函数
 */
void start_process(void* filename_){
    void* function = filename_;
    // 该函数是在当前用户进程已经开始运行时才被调用，因此running_thread返回的就是用户进程对应的内核线程
    task_struct_t *cur = running_thread();
    cur->self_kstack += sizeof(thread_stack_t);

    // 伪装从用户态中断进入到内核
    intr_stack_t* proc_stack = (intr_stack_t *)cur->self_kstack;
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;                                                                     // 不允许用户态直接访问显存, 所以用户态程序如果直接调用console_print就会报错
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;                     // 设置ss的备份ds, es, fs
    proc_stack->eip = function;                                                             // 设置中断返回的CS:EIP
    proc_stack->cs = SELECTOR_U_CODE;                                                       // 设置中断返回的CS:EIP
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);                        // IF=1, 伪装成发生中断
    proc_stack->esp = (void*) ((uint32_t) get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE); // 为虚拟页分配对应的物理页的同时，将ESP指向得到的页的页底
    proc_stack->ss = SELECTOR_U_DATA;                                                       // 设置SS

    // 中断返回, 这里proc_stack放在哪都无所谓，所以用通用约束，并且调用函数前清除寄存器缓存
    asm volatile (
        "movl %0, %%esp;"
        "jmp intr_exit"
        : 
        : "g" (proc_stack)
        : "memory"
    );
}


/**
 * @brief page_dir_activate用于重新设置页目录寄存器cr3
 * 
 * @param tcb 要安装页表寄存器的进程
 */
void page_dir_activate(task_struct_t *tcb){
    // 内核页表在0x100000
    uint32_t pagedir_phy_addr = 0x100000;
    if (tcb->pgdir != NULL){
        // 用户进程则有自己的页目录表A
        pagedir_phy_addr = addr_v2p((uint32_t) tcb->pgdir);
    }

    // 更新页目录寄存器cr3，cr3必须是寄存器才能够赋值的，最后清除寄存器缓存
    asm volatile (
        "movl %0, %%cr3"
        :
        : "r" (pagedir_phy_addr)
        : "memory"
    );
}

/**
 * @brief process_activate用于激活一个进程
 * 
 * @details 该函数具体干的事情:
 * 
 *      1. 将内核位于0x0010_0000的页目录表切换为用户的页目录表
 *      2. 将用户栈替换到tss中的esp0中, 以便于稍后调用
 * 
 * @param tcb 指向要激活的线程
 */
void process_activate(task_struct_t *tcb){
    ASSERT(tcb != NULL);
    // 激活该进程或线程的页表
    page_dir_activate(tcb);

    // 如果稍后要运行的线程是用户线程的话，由于当前还是在0特权级下，只有等下intr_exit之后才是3特权级
    // 所以如果稍后要运行的线程是用户线程的话，则需要更新tss中的esp0
    if (tcb->pgdir){
        update_tss_esp(tcb);
    }
}


/**
 * @brief create_page_dir用于为用户进程创建页目录表
 * 
 * @return uint32_t* 若创建成功, 则返回创建成功的页目录表首字节的物理地址; 失败则返回NULL
 */
uint32_t* create_page_dir(void){
    // 用户进程的页目录依旧需要一个物理页来存储
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if (page_dir_vaddr == NULL){
        console_put_str("create_page_dir: get_kernel_ failed!\n");
        return NULL;
    }
    // 复制页目录表的内核部分, 0x300就是第768项目, 一个页目录项占用4字节
    // 所以从768 * 4 = 0xC00处开始复制, 复制到用户
    // 768 ~ 1024一共256项, 每个页目录项4字节, 所以一共要复制1024个字节
    memcpy(
        (uint32_t*) ((uint32_t) page_dir_vaddr + 0x300 * 4), 
        (uint32_t*) (0xFFFFF000 + 0x300 * 4),
        1024
    );

    // 由于cr3寄存器中需要保存页目录表的物理地址，因此需要将页目录表的虚拟地址转为物理地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t) page_dir_vaddr);
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;

    return page_dir_vaddr;
}


/**
 * @brief create_user_vaddr_bitmap用于给user_prog指向的用户进程创建虚拟内存位图
 * 
 * @param user_prog 需要创先虚拟内存位图的用户进程
 */
void create_user_vaddr_bitmap(task_struct_t *user_prog){
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    // 计算存储描述用户程序所占用的内存需要的bitmap占用的页数。用户程序中给出的地址虽然是虚拟地址, 但是每个虚拟地址都是和一个物理地址一一对应的
    // 因此, 也需要对虚拟地址进行管理, 依旧是使用位图来管理。
    // 由于位图要在物理内存中存储，因此这里先计算描述用户程序所占用内存的位图需要的位图占用页数
    uint32_t bitmap_pg_cnt = DIV_CEILING((0xC0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_byte_len = (0xC0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}


/**
 * @brief process_execute用于创建用户进程
 * 
 * @param filename 要
 * @param name 要创建的用户进程的名字
 */
void process_execute(void *filename, char *name){
    // 初始化内核线程
    task_struct_t *tcb = get_kernel_pages(1);
    init_thread(tcb, name, default_time_slice);
    create_user_vaddr_bitmap(tcb);
    // schedule调度的时候, 实际上运行的第一个命令就是start_process(filename)
    thread_create(tcb, start_process, filename);
    tcb->pgdir = create_page_dir();

    // 操作共享变量，必须要保证操作的原子性
    intr_status_t old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &tcb->general_tag));
    list_append(&thread_ready_list, &tcb->general_tag);
    ASSERT(!elem_find(&thread_all_list, &tcb->all_list_tag));
    list_append(&thread_all_list, &tcb->all_list_tag);
    intr_set_status(old_status);
}