#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H

#include "global.h"
#include "thread.h"

// 默认每个进程的时间片为31个时钟中断
#define default_time_slice 31
#define USER_STACK3_VADDR (0xC0000000 - 0x1000)
// 用户程序起始虚拟地址, 大部分Linux程序编译出来起始地址都是0x8048000附近
#define USER_VADDR_START 0x8048000

// 在kernel.S中声明
extern void intr_exit(void);

/**
 * @brief start_process用于构建用户进程初始上下文，原理就是伪装从用户态中断进入内核运行。
 *      具体来说就是在create_thread中为用户进程在该页的页底分配了一个intr_stack_t来伪装中断压栈。
 *      此外，该函数是在当前用户进程已经开始运行时才被调用
 * 
 * @param filename_ 要启动的用户进程的路径
 * 
 * @note 目前由于还没有实现文件系统，因此目前只能运行单纯的函数
 */
void start_process(void* filename_);


/**
 * @brief process_activate用于激活一个进程
 * 
 * @param tcb 指向要激活的线程
 */
void process_activate(task_struct_t *tcb);


/**
 * @brief page_dir_activate用于重新设置页目录寄存器cr3
 * 
 * @param tcb 要安装页表寄存器的进程
 */
void page_dir_activate(task_struct_t *tcb);


/**
 * @brief create_page_dir用于为用户进程创建页目录表
 * 
 * @return uint32_t* 若创建成功, 则返回创建成功的页目录表首字节的物理地址; 失败则返回NULL
 */
uint32_t* create_page_dir(void);


/**
 * @brief create_user_vaddr_bitmap用于给user_prog指向的用户进程创建虚拟内存位图
 * 
 * @param user_prog 需要创先虚拟内存位图的用户进程
 */
void create_user_vaddr_bitmap(task_struct_t *user_prog);


/**
 * @brief process_execute用于创建用户进程
 * 
 * @details process_execute创建用户进程的流程如下:
 * 
 *      => 调用process_execute时立即运行:
 *          1. 创建一个内核线程，该内核线程将用于在3特权级下运行用户进程
 *              1.1 调用get_kernel_pages在内核存储空间中分配一个页, 用于存储TCB
 *              1.2 调用init_thread来初始化内核线程的TCB, 此时仅填充页顶部的内存区域
 *          2. 调用create_vaddr_bitmap来为用户虚拟内存创建位图. 
 *             因为用户进程将使用自己的虚拟空间的0~3GB, 也需要使用位图来进行管理
 *             此外, 用户进程虚拟内存的位图将保存在内核线程TCB中, 以和一般的内核线程表示区别
 *          3. 调用create_thread初始化内核线程栈
 *              3.1 创建intr_stack_t以伪装从用户进程中断进入到内核
 *              3.2 创建thread_stack_t以使得内核线程第一次被调度上CPU后能正常运行, 而后中断返回到特权3下的用户进程
 *          4. 为用户进程创建页目录表, 因为每个用户进程都有独立的虚拟地址
 *             而相同虚拟地址不同用户进程对应物理地址都不同, 因此每个用户进程都要有自己的页目录表
 *              4.1 内核中分配一个页, 用于存储用户进程的页目录表
 *              4.2 将内核页目录表768~1024项复制到用户页目录表, 以使得用户虚拟地址3~4G部分都指向内核, 以支持后续的系统调用
 *              4.3 设置用户页目录表最后一项(1023)指向用户页目录表自己
 *          5. 将用于运行用户进程的内核线程插入到就绪队列和所有队列
 * 
 *      => (时钟)中断发生后运行:
 *          1. 外部源发送中断到8259A
 *          2. 8259A通知CPU发生了某个中断
 *          3. CPU收到8259A发来的消息，得知发生中断
 *          4. CPU发消息到8259A以获得中断的中断号
 *          5. 8259A收到CPU的消息，将中断号发送给CPU
 *          6. CPU收到中断号，将确认收到消息发送给8259A
 *          7. CPU从idt中根据中断号去获取中断门
 *          8. 中断门中保存了中断处理程序中断处理程序的地址、中断门权限、目的表代码段权限，这个中断处理程序地址在kernerl.S中，由汇编实现
 *          9. 汇编中实现的中断处理程序：
 *                  9.1 压入错误代码                                                            (CPU完成部分)
 *                  9.2 压入上下文：ds, es, fs, gs, eax, ecx, edx, ebx, esp, ebp, esi, edi      (用户完成)
 *          10. 执行C语言的中断处理程序, 若为时钟中断, 则执行时钟中断处理程序(timer.c)
 *                  10.1 若当前内核线程的时间片没有用完, 则继续运行
 *                  10.2 若当前内核线程时间片已经用完, 则调用schedule函数进行调度
 *          11. schedule中干的事:
 *                  11.1 将当前内核线程的状态从TASK_RUNNING改为TASK_READY
 *                  11.2 从就绪队列中选取得到一个新的内核线程, 将新内核线程的状态设置为TASK_RUNNING
 *                  11.3 调用process_activate激活内核线程
 *                  11.4 保护旧内核线程数据到内存中, 加载新内核线程数据到寄存器中
 *          12. process_activate中干的事:
 *                  12.1 安装页目录表:
 *                      A. 如果是一般内核线程, 则将cr3设置为0x0010_0000, 即内核页目录表的地址
 *                      B. 如果是将运行用户线程的内核线程, 则将cr3设置为用户页目录表的物理地址
 *                  12.2 切换栈寄存器(用户程序3, 内核线程0), 开机之后, 系统处于0特权级之下, 中断号运行的新线程有两种可能:
 *                      A. 运行一个新的内核线程, 此时不需要进行SS:ESP和SS0:ESP0的切换
 *                      B. 运行一个用户进程, 此时由于假装从中断返回, 因此会发生SS:ESP和SS0:ESP0的互换
 *                         所以需要提前把用户进程的栈存入到TSS的esp0中去
 * 
 * @param filename 用户程序的路径
 * @param name 要创建的进程的名字
 */
void process_execute(void *filename, char *name);

#endif