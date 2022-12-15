#ifndef __DEVICE_TIMER_H
#define __DEVICE_TIMER_H
#include "stdint.h"
void timer_init(void);

void intr_timer_handler(void);

/**
 * @brief mtime_sleep用于以毫秒为单位进行睡眠, 1s = 1000ms
 * 
 * @param m_seconds 需要睡眠的毫秒数
 */
void mtime_sleep(uint32_t m_seconds);
#endif