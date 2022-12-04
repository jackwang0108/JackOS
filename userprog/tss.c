#include "tss.h"
#include "global.h"
#include "thread.h"
#include "print.h"
#include "string.h"


/**
 * @brief TSS结构
 * 
 * @details 
 *      TSS的作用:
 *          CPU执行的时候当前的代码段的选择子中有RPL, 表示了当前的权限. 所以CS中的RPL才是权限的根本.
 *          CPU在执行指令的时候, 只会取指执行, 因此单纯的jmp, 栈什么的都是不变的, 所以就可能造成数据泄露
 *          因此, TSS中保存了三组寄存器: (ss0, esp0), (ss1, esp1), (ss2, esp2), 再加上ss, esp寄存器, 分别对应了四个特权级的代码适用的栈
 *          所以TSS的作用就是在不同特权级跳转的时候, 使用不同的特权级的栈
 *      
 *      =>从低特权级到高特权级
 *          从低特权级到高特权级只能通过call或者jmp中断门/调用门这种方式来实现. 此时中断门/调用门指向的目标代码段是高特权级的
 *          此时跳转流程为:
 *          A. 中断门: 我们主要用的方式
 *              A.1 外部中断到8259A
 *              A.2 8259A通知CPU发生了某个中断
 *              A.2 CPU收到8259A发来的消息，得知发生中断
 *              A.3 CPU发消息到8259A以获得中断的中断号
 *              A.3 8259A收到CPU的消息，将中断号发送给CPU
 *              A.4 CPU收到中断号，将确认收到消息发送给8259A
 *              A.5 CPU从idt中根据中断号去获取中断门
 *                  A.5.1 中断门中保存了: 中断处理程序的地址(CS:EIP)、中断门权限、目的表代码段权限
 *              A.6 CPU将CS:EIP设置为中断处理程序的地址
 *              A.7 CPU发现目标代码段时高特权级的, 因此CPU把TSS中高特权级栈的ss0:esp0和(当前低特权级的)ss:esp互换
 *          B. 调用门: 用不着, 不讲
 * 
 *      => 从高特权级到低特权级
 *          A. 只能通过中断返回
 * 
 * 因此, 我们这里使用TSS的目的, 就是把内核和用户进程的SS:ESP区分开
 */
typedef struct __tss_t {
    uint32_t    backlink;          ///< 指向上一个任务的TSS指针，即上一个任务的TSS的地址
    uint32_t*   esp0;
    uint32_t    ss0;
    uint32_t*   esp1;
    uint32_t    ss1;
    uint32_t*   esp2;
    uint32_t    ss2;
    uint32_t    cr3;
    uint32_t    (*eip)(void);
    uint32_t    eflags;
    uint32_t    eax;
    uint32_t    ecx;
    uint32_t    edx;
    uint32_t    ebx;
    uint32_t    esp;
    uint32_t    ebp;
    uint32_t    esi;
    uint32_t    edi;
    uint32_t    es;
    uint32_t    cs;
    uint32_t    ss;
    uint32_t    ds;
    uint32_t    fs;
    uint32_t    gs;
    uint32_t    ldt;
    uint32_t    trace;
    uint32_t    io_base;
} tss_t;

static tss_t tss;   ///< 全局的TSS


/**
 * @brief update_tss_esp用于将tss中esp0的值设置为tcb的栈.
 * 
 * @details
 *      Linux中, 权限只用了0和3. 无论是:
 *          1. 从0的内核返回到3的用户程序
 *          3. 从3的用户程序跳转到0的内核
 *      都需要保存当前运行的栈到esp0中去
 * 
 * @param tcb tcb的值将被设置为esp0
 */
void update_tss_esp(task_struct_t *tcb){
    tss.esp0 =  (uint32_t*) ((uint32_t)tcb + PG_SIZE);
}

/**
 * @brief make_gdt_desc用于创建gdt描述符
 * 
 * @param desc_addr 段描述符中的段基地址
 * @param limit 段描述符中的段界限
 * @param attr_low 段描述符中
 * @param attr_high 
 * @return gdt_desc_t 
 * 
 * @note 32位系统下一个段描述符八个字节, 具体结构请看global.h
 */
static gdt_desc_t make_gdt_desc(uint32_t *desc_addr, uint32_t limit, uint8_t attr_low, uint8_t attr_high){
    uint32_t desc_base = (uint32_t) desc_addr;
    // desc不能是指针，必须是结构，因为指针又是在另外的内存单元了
    gdt_desc_t desc;
    desc.limit_low_word = limit & 0x0000FFFF;
    desc.base_low_word = desc_base & 0x0000FFFF;
    desc.base_mid_byte = ((desc_base & 0x00FF0000) >> 16);
    desc.attr_low_byte = (uint8_t) (attr_low);
    desc.limit_high_attr_high = (((limit & 0x000F0000) >> 16) + (uint8_t)(attr_high));
    desc.bsae_high_byte = desc_base >> 24;
    return desc;
}


/**
 * @brief tss_init用于在GDT中初始化tss描述符
 * 
 */
void tss_init(void){
    put_str("tss_init start\n");
    // 将tss占用的内存清0
    uint32_t tss_size = sizeof(tss);
    memset(&tss, 0, tss_size);
    tss.ss0 = SELECTOR_K_STACK;
    tss.io_base = tss_size;                 // 不设置IO位图

    // gdt段基地址0x900, tss放到第四个段, 一个段八个字节, 而第一个段要留空以让CPU进行未初始化检查
    // 所以tss位置就在 0x900 + 8 * (1 + 3) = 0x900 + 0x20 = 0x920
    *((gdt_desc_t *)0xC0000920) = make_gdt_desc((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);

    // 构建用户段选择子 --> 用户代码段
    *((gdt_desc_t *)0xC0000928) = make_gdt_desc((uint32_t*)0, 0xFFFFF, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
    // 构建用户段选择子 --> 用户数据段
    *((gdt_desc_t *)0xC0000930) = make_gdt_desc((uint32_t*)0, 0xFFFFF, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);

    // 准备更新gdtr寄存器, 首先计算操作数
    uint64_t gdt_operand = ((8 * 7 - 1) | ((uint64_t) (uint32_t)0xC0000900 << 16));     // 目前只有1 + 6 = 7个段

    
    asm volatile ("lgdt %0" : : "m" (gdt_operand));                 // 加载gdtr寄存器
    asm volatile ("ltr %w0" : : "r" (SELECTOR_TSS));                 // 加载tr寄存器， TSS除了要在GDT中注册以外，还必须要保存在TR中
    put_str("test_init done\n");
}