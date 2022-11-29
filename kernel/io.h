/**
 * @file io.h
 * @author Shihong Wang
 * @brief 利用扩展内联汇编实现了C语言访问硬件IO的能力
 * @version 0.1
 * @date 2022-11-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __KERNEL_IO_H
#define __KERNEL_IO_H

#include "stdint.h"

// C只能精确到地址、变量，而汇编却能够精确到位数，所以C内联汇编中需要使用机器模式来指定位数
// b -- 输出寄存器的QImode, 即寄存器的低八位，即使用 [a-d]l
// w -- 输出寄存器的HImode, 即寄存器的低两字节，即使用 [a-d]x
// HImode 表示 Half-Integer Mode, C标准中要求一个Integer 4字节，所以HImode指的就是低两字节
// QImode 表示 Quarter-Integer Mode, 同上，QImode指的就是低一字节

/**
 * @brief outb (out byte)用于向指定端口输出一个字节的数据
 * @param port 端口号，范围0~255
 * @param data 输出的数据，一个字节
 */
static inline void outb(uint16_t port, uint8_t data){
    // 虽然16位支持端口范围0~65536，但是N表示立即数约束，即编译得到的指令是mov bx, data(data就是立即数)
    // outb指令一次只能写一个字节，数据需要放在al中
    asm volatile ( "outb %b0, %w1" : : "a"(data), "Nd" (port) );
}

/**
 * @brief outsw (out series word)用于将addr处开始的word_cnt个字写入到port端口中
 * 
 * @param port 端口号，范围0~65535
 * @param addr 将要写入的内存区域的首地址
 * @param word_cnt 写入的字节数
 */
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt){
    // +表示寄存器可读写，S将映射到esi/si
    asm volatile (
        "cld; rep outsw"\
        : "+S" (addr), "+c" (word_cnt)\
        : "d" (port)
    );
}


/**
 * @brief inb (in byte)从端口port中读取一个字节
 * 
 * @param port 端口号，范围0~255
 * @return uint8_t 读取的数据
 */
static inline uint8_t inb(uint16_t port){
    uint8_t data;
    // = 表示只写
    asm volatile ("inb %w1, %b0" : "=a"(data) : "Nd" (port));
    return data;
}


/**
 * @brief insw (in series byte)从端口port中读入word_cnt个字节到addr指向的内存区域
 * 
 * @param port 端口号，范围0~65535
 * @param addr 将写入的内存区域
 * @param word_cnt 读取的字节数
 */
static inline void insw(uint16_t port, void *addr, uint32_t word_cnt){
    // +表示读写，D表示映射到edi、di，由于写入了内存，所以最后的clobber/modify要说明修改了内存
    asm volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}

#endif