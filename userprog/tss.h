#ifndef __USERPROG_TSS_H
#define __USERPROG_TSS_H

#include "stdint.h"
#include "thread.h"

void tss_init(void);

/**
 * @brief update_tss_esp用于将tss中esp0的值设置为tcb的栈
 * 
 * @param tcb tcb的值将被设置为esp0
 */
void update_tss_esp(task_struct_t *tcb);

#endif