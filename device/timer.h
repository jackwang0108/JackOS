#ifndef __DEVICE_TIMER_H
#define __DEVICE_TIMER_H
#include "stdint.h"
void timer_init(void);

void intr_timer_handler(void);
#endif