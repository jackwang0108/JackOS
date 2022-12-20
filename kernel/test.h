#ifndef __KERNEL_TEST_H
#define __KERNEL_TEST_H

#include "fs.h"
#include "ide.h"
#include "dir.h"
#include "debug.h"
#include "stdio.h"
#include "kstdio.h"
#include "string.h"
#include "global.h"
#include "thread.h"
#include "memory.h"
#include "syscall.h"
#include "process.h"
#include "interrupt.h"


extern list_t partition_list;
extern partition_t *current_partition;

// thread test
void k_thread_a(void *);
void k_thread_b(void *);
void u_prog_a(void);
void u_prog_b(void);
void u_prog_c(void);
void test_user_porg(void);
void test_kernel_thread(void);

// memory test
void test_memory(void);

// file system test
void test_create_close_unlink(void);
void test_write_read_lseek(void);
void test_mkdir_rmdir(void);
void test_open_read_close_dir(void);
void test_getcwd_chdir(void);
void test_stat(void);


void test_all(void);

#endif