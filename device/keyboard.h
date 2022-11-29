#ifndef __DEVIDC_KEYBOARD_H
#define __DEVIDC_KEYBOARD_H

#include "ioqueue.h"

extern ioqueue_t kbd_buf;

void keyboard_init(void);

#endif