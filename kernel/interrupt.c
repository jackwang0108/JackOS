#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "print.h"
#include "io.h"


// 宏声明在此处避免污染头文件
#define PIC_M_CTRL 0x20                     // master 8259A control port
#define PIC_M_DATA 0x21                     // master 8259A data port
#define PIC_S_CTRL 0xA0                     // slave 8259A control port
#define PIC_S_DATA 0xA1                     // slave 8259A data port

// 目前支持的中断总数，每一个中断都要有一个中断处理程序处理，所以要有对应的中断门
// 此外，0x1F是CPU保留的中断号（目前只用了19个），从0x20个开始，是给用户使用的
#define IDT_DESC_CNT 0x81

/// @brief 系统调用的入口函数, 定义在kernel.S中
/// @param 无参数
/// @return uint32_t 系统调用的返回值
extern uint32_t syscall_handler(void);


#define EFLAGS_IF   0x00000200              // eflags寄存器中，if位为1
#define GET_EFLAGS(EFLAGS_VAR) asm volatile ("pushfl; popl %0;" : "=g"(EFLAGS_VAR))

// 中断门描述符结构体
typedef struct __gate_desc {
    uint16_t    func_offset_low_word;       // 中断处理程序在目标段内的偏移的0~15位
    uint16_t    selector;                   // 中断处理程序目标代码段的选择子
    uint8_t     dcount;                     // 保留未使用
    uint8_t     attribute;                  // 中断门属性
    uint16_t    func_offset_high_word;      // 中断处理程序在目标段内的偏移的16~31位
} gate_desc_t;


static void make_idt_desc(gate_desc_t *p_gdesc, uint8_t attr, intr_handler function);
static gate_desc_t idt[IDT_DESC_CNT];

char* intr_name[IDT_DESC_CNT];                                          // 异常的名字 
intr_handler idt_table[IDT_DESC_CNT];                                   // 中断处理程序入口地址，
extern intr_handler intr_entry_table[IDT_DESC_CNT];                     // 中断处理程序入口地址，在kernel.S中定义，最终将指向idt_table中的异常处理程序


/**
 * @brief pic_init用于初始化可编程中断控制器(Programmable Interrupt Controller)
 * 
 * @note 未来的HLA层需要在这里添加，目前只支持8259A
 */
static void pic_init(void){
    // 初始化主片
    outb(PIC_M_CTRL, 0b00010001);               // ICW1: 边沿触发，级联8259，需要ICW4
    outb(PIC_M_DATA, 0x20);                     // ICW2: 起始中断向量号设置为0x20, IR[0~7] 的中断号设置为0x20~0x27
    outb(PIC_M_DATA, 0x04);                     // ICW3: IR2接从片
    outb(PIC_M_DATA, 0b00000001);               // ICW4: 8086，正常EOI

    // 初始化从片
    outb(PIC_S_CTRL, 0b00010001);               // ICW1: 边沿触发，级联8259，需要ICW4
    outb(PIC_S_DATA, 0x28);                     // ICW2: 起始中断向量号设置为0x20, IR[8~15] 的中断号设置为0x28~0x2F
    outb(PIC_S_DATA, 0x02);                     // ICW3: 从片接到主片IR2引脚
    outb(PIC_S_DATA, 0b00000001);               // ICW4: 8086，正常EOI

    /*
     *  打开主片:
     *      1. IR0, 开启时钟中断
     *      2. IR1, 键盘中断
     *      3. IR2, 级联从片, 8259A从片的中断是由8259A主片帮忙向处理器传达的, 
     *         8259A从片是级联在8259A主片的IRQ2接口的, 因此为了让处理器也响应来自8259A从片的中断,
     *         主片屏蔽中断寄存器必须也把IRQ2打开
     *  打开从片:
     *      1. IRQ14, 硬盘控制器中断
    */
    outb(PIC_M_DATA, 0b11111000);
    outb(PIC_S_DATA, 0b10111111);

    put_str("    pic_init done\n");
}


/**
 * @brief make_idt_desc用于制作一个中断门, 并将其安装到指定位置
 * 
 * @param p_gdesc 中断门将要安装到的位置, 必须是指向中断描述符表中的一个地址: &idt[XXX]
 * @param attr 中断门属性
 * @param function 中断门指向的函数
 */
static void make_idt_desc(gate_desc_t *p_gdesc, uint8_t attr, intr_handler function){
    p_gdesc->func_offset_low_word = (uint32_t) function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t) function & 0xFFFF0000) >> 16;
}


/**
 * @brief 用于初始化中断描述符表(中断门表)
 * 
 * @details idt_desc_init将会:
 *              1. 初始化一般中断门(中断描述符)idt[i]中的目标代码段地址为intr_entry_table[i]中的地址, 并且将其访问特权级DPL设置为0
 *              2. 初始化系统中断门的目标代码段地址为syscall_handler, 并将其访问特权级DPL设置为3
 */
static void idt_desc_init(void){
    for (int i = 0; i < IDT_DESC_CNT; i++)
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    // 系统中断门(中断描述符)DPL是3, 这样用户态的程序才能够通过中断访问该段, 此外, 单
    make_idt_desc(&idt[IDT_DESC_CNT - 1], IDT_DESC_ATTR_DPL3, syscall_handler);
    put_str("    idt_desc_init done\n");
}


/**
 * @brief 通用中断处理程序，仅输出中断号
 * 
 * @param vec_nr 中断号
 */
static void general_intr_handler(uint8_t vec_nr){
    // IRQ7和IRQ15会产生伪中断，IRQ7和IRQ15被映射到0x27和0x2F
    if (vec_nr == 0x27 || vec_nr == 0x2F)
        return;

    // 输出中断信息
    int cursor_pos = 0;
    // 屏幕左上角清理出一块空白区域，用于打印异常信息
    set_cursor(cursor_pos);
    while (cursor_pos < 320){
        put_char(' ');
        cursor_pos++;
    }
    
    // 输出信息
    set_cursor(0);
    put_str("!!!!!!!!!!     Exception Message Begin     !!!!!!!!!!");
    set_cursor(88);
    // 输出中断信息
    put_str(intr_name[vec_nr]), put_str(": 0x"), put_int(vec_nr), put_char('\n');
    // 14号中断是缺页中断
    if (vec_nr == 14){
        int page_fault_vaddr = 0;
        // 缺页中断发生后，CPU会将缺页的地址放到CR2上
        asm volatile (
            "movl %%cr2, %0"
            : "=r" (page_fault_vaddr)
        );
        put_str("Page Fault Addr: "), put_int(page_fault_vaddr);
    } else {
    }
    put_char('\n');
    put_str("!!!!!!!!!!     Exception Message End     !!!!!!!!!!");
    // 中断发生后，CPU自动关闭中断，所以此后程序不会再继续运行
    while (1);
}


/**
 * @brief register_handler用于将中断处理程序注册到idt_table中
 * 
 * @param vector_no 要注册的中断处理程序
 * @param function 中断处理程序的入口地址
 */
void register_handler(uint8_t vector_no, intr_handler function){
    idt_table[vector_no] = function;
}

/**
 * @brief exception_init用于初始化中断处理程序
 * 
 */
static void exception_init(void){
    for (int i = 0; i < IDT_DESC_CNT; i++){
        idt_table[i] = general_intr_handler;
        intr_name[i] = "unknown";
    }
    // 0~0x1F都是CPU保留
    intr_name[0] = "#DE Division Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceed Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 是 intel 保留项
    intr_name[16] = "#MF x86 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";

    // 0x20以后是给用户用的
    intr_name[0x20] = "Timer Interrupt";
}

/**
 * @brief idt_init用于初始化中断描述符表
 * 
 */
void idt_init(){
    put_str("idt_init start\n");
    idt_desc_init();                        // init idt
    exception_init();                       // 异常名初始化并且注册中断处理函数
    pic_init();                             // init 8259A
    
    // 加载idt, idtr高32位是idt基地址, 低16位是16位的表界限
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    asm volatile ("lidt %0" : : "m" (idt_operand));
    put_str("    idtr loaded\n");
    put_str("idt_init done\n");
}



/**
 * @brief 开中断，并且返回中断之前的状态
 * 
 * @return intr_status_t 中断之前的状态
 */
intr_status_t intr_enable(void){
    intr_status_t old_status;
    if (INTR_ON == intr_get_status()){
        old_status = INTR_ON;
        return old_status;
    } else {
        old_status = INTR_OFF;
        asm volatile ("sti");               // 开中断，sti将IF设置为1
        return old_status;
    }
}

/**
 * @brief 关中断，并且返回中断之前的状态
 * 
 * @return intr_status_t 中断之前的状态
 */
intr_status_t intr_disable(){
    intr_status_t old_status;
    if (INTR_ON == intr_get_status()){
        old_status = INTR_ON;
        asm volatile ("cli" : : : "memory");
        return old_status;
    } else {
        old_status = INTR_OFF;
        return old_status;
    }
}

/**
 * @brief 设置中断状态
 * 
 * @param status 将设置的中断状态
 * @return intr_status_t 之前的中断状态
 */
intr_status_t intr_set_status(intr_status_t status){
    return status & INTR_ON ? intr_enable() : intr_disable();    
}

/**
 * @brief intr_get_status用于获取当前中断的状态
 * 
 * @return intr_status_t 当前中断的状态
 */
intr_status_t intr_get_status(){
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}