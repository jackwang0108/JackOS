#include "fs.h"
#include "exec.h"
#include "kstdio.h"
#include "string.h"
#include "thread.h"
#include "memory.h"


extern void intr_exit(void);

/**
 * @brief segment_load用于将fd指向的文件中偏移为offset, 大小为filesz的段加载到虚拟地址为
 *        vaddr所对应的物理内存处. 该函数会自动为分配一个物理页, 并完成与vaddr表示的虚拟页的映射
 * 
 * @param fd 需要加载的程序文件的文件描述符
 * @param offset 段在文件内的偏移地址
 * @param filesz 段大小
 * @param vaddr 加载到内存中的虚拟地址
 * @return true 加载成功
 * @return false 加载失败
 */
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr){
    // 计算段将要加载到的虚拟页
    uint32_t vaddr_first_page = vaddr & 0xFFFFF000;
    // 计算段加载到虚拟页后在将在虚拟页内占用的字节数
    uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000FFF);

    
    // 如果虚拟页内装不下, 则计算额外需要的页数
    uint32_t occupy_pages = 0;
    if (filesz > size_in_first_page){
        uint32_t left_size = filesz - size_in_first_page;
        occupy_pages = DIV_CEILING(left_size, PG_SIZE) + 1;
    } else {
        occupy_pages = 1;
    }

    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_first_page;
    while (page_idx < occupy_pages){
        // 获得虚拟页在页目录中的页表地址
        uint32_t *pde = pde_addr(vaddr_page);
        // 获得虚拟页的物理地址
        uint32_t *pte = pte_addr(vaddr_page);
        // 如果存储页表的物理页或者虚拟页对应的物理页不存在, 就得分配一个页, 然后完成绑定
        if (!(*pde & 0x00000001) || !(*pte & 0x00000001))
            if (get_a_page(PF_USER, vaddr_page) == NULL)
                return false;
        vaddr_page += PG_SIZE;
        page_idx++;
    }
    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void*) vaddr, filesz);
    return true;
}


/**
 * @brief load用于将filename指向的程序文件加载到内存中
 * 
 * @param pathname 需要加载的程序文件的名称
 * @return int32_t 若加载成功, 则返回程序的起始地址(虚拟地址); 若加载失败, 则返回-1
 */
static int32_t load(const char* pathname){
    int32_t ret = -1;

    Elf32_Ehdr elf_header;
    Elf32_Phdr prog_header;
    memset(&elf_header, 0, sizeof(Elf32_Ehdr));

    // 打开文件
    int32_t fd = sys_open(pathname, O_RDONLY);
    if (fd == -1)
        return -1;

    // 读取文件内容
    if (sys_read(fd, &elf_header, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)){
        ret = -1;
        goto done;
    }

    // 校验elf头, check elf header
    if (
        // octal 177 = 0x7f, 0x7f + ELF is elf magic number.
        // \1\1\1 means 32-bit elf file, LSB, current version separately
        memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7)
        // e_type == 2, executable file
        || elf_header.e_type != 2
        // e_machine == 3, Intel 80386
        || elf_header.e_machine != 3
        // e_version == 1, fix to 1
        || elf_header.e_version != 1
        // maximum support 1024
        || elf_header.e_phnum > 1024
        // correspond to Elf32_Phdr
        || elf_header.e_phentsize != sizeof(Elf32_Phdr)
    ){
        kprintf("Elf Header check failed!\n");
        ret = -1;
        goto done;
    }


    Elf32_Off prog_header_offset = elf_header.e_phoff;
    Elf32_Half prog_header_size = elf_header.e_phentsize;

    // 遍历所有程序头表
    uint32_t prog_idx = 0;
    while (prog_idx < elf_header.e_phnum){
        // 清0
        memset(&prog_header, 0, prog_header_size);
        // 移动文件指针
        sys_lseek(fd, prog_header_offset, SEEK_SET);
        // 只获取文件头
        if (sys_read(fd, &prog_header, prog_header_size) != prog_header_size){
            ret = -1;
            goto done;
        }

        // 可加载段直接调用segment_load加载到内存
        if (PT_LOAD == prog_header.p_type){
            if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)){
                ret = -1;
                goto done;
            }
        }

        // 移动到下一个程序头偏移
        prog_header_offset += elf_header.e_phentsize;
        prog_idx++;
    }
    
    ret = elf_header.e_entry;

done:
    sys_close(fd);
    return ret;
}


/**
 * @brief sys_execv是execv系统调用的实现函数. 用于将path指向的程序加载到内存中, 而后
 *        用该程序替换当前程序
 * 
 * @param path 程序的绝对路径
 * @param argv 参数列表
 * @return int32_t 若运行成功, 则返回0 (其实不会返回); 若运行失败, 则返回-1
 */
int32_t sys_execv(const char* path, const char *argv[]){
    // 计算参数个数
    uint32_t argc = 0;
    while (argv[argc++]);
    argc--;

    // 加载程序到内存
    int32_t entry_point = load(path);
    if (entry_point == -1){
        kprintf("%s: load %s into memory failed!\n", __func__, path);
        return -1;
    }

    // 修改进程信息
    task_struct_t *cur = running_thread();
    // 修改进程名
    memcpy(cur->name, path, TASK_NAME_LEN);
    cur->name[TASK_NAME_LEN - 1] = 0;

    // 伪装中断返回, 从而使得能够执行用户进程
    intr_stack_t *intr_0_stack = (intr_stack_t *) ((uint32_t)cur + PG_SIZE - sizeof(intr_stack_t));
    // 传参, 参数放在ebx和ecx中
    intr_0_stack->ebx = (int32_t) argv;
    intr_0_stack->ecx = argc;
    // 修改终端返回地址
    intr_0_stack->eip = (void *)entry_point;
    // 修改用户进程的栈地址为用户空间最高处
    intr_0_stack->esp = (void *)(0xC0000000);

    // 直接中断返回, 不知道为什么这里跳转到intr_exit之后, 在popa的时候缺页中断0x0E
    // 运行到这里ss指向TSS, esp指向是对的, 猜测和TSS有关
    asm volatile (
        "movl %0, %%esp;"
        "jmp intr_exit"
        :
        : "g" (intr_0_stack)
        : "memory"
    );

    // Actually, program will never run here. Just make gcc happy when compiling
    return 0;
}