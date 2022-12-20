#include "test.h"


void test_all(void){
    /* -------------------- Test kernel thread -------------------- */
    // test_kernel_thread();


    /* -------------------- Test memory -------------------- */
    test_memory();


    /* ---------------------- Test user prog ---------------------- */
    // test_user_porg();

    /* --------------------- Test file system --------------------- */
    // 1. Test for creating/opening, closing and removing a file.
    // test_create_close_unlink();      
    // 2. Test for writing and reading a file.
    // test_write_read_lseek();
    // 3. Test for creating and removing a directory.
    // test_mkdir_rmdir();
    // 4. Test for opening, reading and closing a directory.
    // test_open_read_close_dir();
    // 5. Test for 'pwd' and 'cd' system call 
    // test_getcwd_chdir();
    // 6. Test for 'ls' system call
    // test_stat();
}

void k_thread_a(void *arg){
    char* para = (char *)arg;

    void *addrs[7];
    kprintf("thread a_start\n");
    int max = 1000;
    while (max-- > 0){
        int i, size;
        // alloc test 1
        for (i = 0, size = 128; i < 5; i++, size *= 2)
            addrs[i] = sys_malloc(size);
        
        for (i = 0; i < 5; i++)
            sys_free(addrs[i]);
    }
    kprintf("thread a_ends\n");

    while(1);
}

void k_thread_b(void *arg){
    char* para = (char *)arg;
    void *addrs[9];
    int max = 1000;
    kprintf("thread b_start\n");
    while (max-- > 0){
        int i, size;
        // alloc test 1
        for (i = 0, size = 128; i < 5; i++, size *= 2)
            addrs[i] = sys_malloc(size);
        
        for (i = 0; i < 5; i++)
            sys_free(addrs[i]);
    }
    kprintf("thread_b end\n");
    while(1);
}



void u_prog_a(void){
    void *vaddrs[3] = {
        malloc(256),
        malloc(255),
        malloc(254)
    };
    printf("prog_a_malloc addr: 0x%x, 0x%x, 0x%x\n", (void*)vaddrs[0], (void*)vaddrs[1], (void*)vaddrs[2]);
    int delay = 1e5;
    while (delay-- > 0);
    for (int i = 0; i < 3; i++)
        free(vaddrs[i]);
    while (1);
}


void u_prog_b(void){
    void *vaddrs[3] = {
        malloc(256),
        malloc(255),
        malloc(254)
    };
    printf("prog_b_malloc addr: 0x%x, 0x%x, 0x%x\n", (void*)vaddrs[0], (void*)vaddrs[1], (void*)vaddrs[2]);
    int delay = 1e5;
    while (delay-- > 0);
    for (int i = 0; i < 3; i++)
        free(vaddrs[i]);
    while (1);
}

void u_prog_c(void){
    int32_t fd = open("/test", O_CREAT);
    close(fd);
    while(1);
}


void test_kernel_thread(void){
    intr_status_t old_status = intr_enable();
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 31, k_thread_b, "argB ");
}


void test_user_porg(void){
    intr_status_t old_status = intr_enable();
    // process_execute(u_prog_a, "user_prog_a");
    // process_execute(u_prog_b, "user_prog_b");
    process_execute(u_prog_c, "user_prog_c");
}


void test_memory(void){
    kprintf("Start memory test...\n");
    uint32_t size = 16;
    uint32_t max_pages = 3;
    char **ptrs;
    while (size < 20480){
        uint32_t ptr_num = 4096 * max_pages / size + 1;
        ptr_num = (ptr_num == 1) ? 2 : ptr_num;
        ptrs = (char**) sys_malloc(sizeof(char*) * ptr_num);
        if (ptrs == NULL)
            kprintf("Malloc for ptrs failed!\n");

        kprintf("Alloc %d bytes for %d times.\n", size, ptr_num);
        kprintf("Writing...\n");
        for (uint32_t j = 0; j < ptr_num; j++){
            ptrs[j] = (char*) sys_malloc(size);
            for (uint32_t i = 0; i < size - 1; i++)
                ptrs[j][i] = 'A';
        }

        kprintf("Releasing...\n");
        for (uint32_t j = 0; j < ptr_num; j++)
            sys_free(ptrs[j]);
        sys_free(ptrs);
        size *= 2;
    }
}


void test_create_close_unlink(void){
    kprintf("/--------------- %s test start ---------------/\n", __func__);
    char *file1 = "/file1";
    char *file2 = "/file2";
    char *file3 = "/file3";

    kprintf("=> Create %s\n", file1);
    int32_t fd1 = sys_open(file1, O_CREAT | O_RDWD);
    kprintf("    => %s sys_open %s return fd: %d\n", file1, fd1 == -1 ? "fail" : "success", fd1);

    kprintf("=> Re-create %s\n", file1);
    int fd_t = sys_open(file1, O_CREAT | O_RDWD);
    kprintf("    => %s sys_open %s return fd: %d\n", file1, fd_t == -1 ? "fail" : "success", fd_t);

    kprintf("=> Create %s\n", file2);
    int32_t fd2 = sys_open(file2, O_CREAT | O_RDWD);
    kprintf("    => %s sys_open %s return fd: %d\n", file2, fd2 == -1 ? "fail" : "success", fd2);

    kprintf("=> Close %s\n", file1);
    int ret = sys_close(fd1);
    kprintf("    => %s sys_close %s, return value: %d\n", file1, ret == -1 ? "fail" : "success", ret);

    kprintf("=> Close %s\n", file2);
    ret = sys_close(fd2);
    kprintf("    => %s sys_close %s, return value: %d\n", file2, ret == -1 ? "fail" : "success", ret);

    kprintf("=> Create %s\n", file3);
    int32_t fd3 = sys_open(file3, O_CREAT | O_RDWD);
    kprintf("    => %s sys_open %s return fd: %d\n", file3, fd3 == -1 ? "fail" : "success", fd3);
    ret = sys_close(fd3);

    kprintf("=> Remove %s\n", file2);
    sys_unlink(file2);

    kprintf("/--------------- %s test done ---------------/\n", __func__);
}



void test_write_read_lseek(void){
    kprintf("/--------------- %s test start ---------------/\n", __func__);
    char *file1 = "/file1";
    char *file2 = "/file2";

    int len = 0;
    int read = 0;
    char buf[64] = {0};
    char *str1 = "Catch me if you can!";
    char *str2 = "Gotcha!";

    kprintf("=> Writing %s\n", file1);
    int32_t fd1;
    kprintf("    => open %s, return fd: %d\n", file1, (fd1 = sys_open(file1, O_CREAT | O_RDWD)));
    if (fd1 != -1){
        len = strlen(str1);
        kprintf("    => writing \"%s\" to %s, string len: %d, %d char written", str1, file1, len, sys_write(fd1, str1, len));
        read = sys_read(fd1, buf, len);
        kprintf("    => reading from %s, %d char read: \"%s\"\n", file1, read, buf);
        kprintf("    => lseek %s, offset: 0, whence: SEEKSET\n", file1);
        sys_lseek(fd1, 0, SEEK_SET);
        read = sys_read(fd1, buf, len);
        kprintf("    => reading from %s, %d char read: \"%s\"\n", file1, read, buf);
        kprintf("    => closing %s, return value: %d\n", file1, sys_close(fd1));
    } else {
        kprintf("    => %s open failed!\n");
    }
    memset(buf, 0, sizeof(buf));

    kprintf("=> Writing %s\n", file2);
    int32_t fd2;
    len = strlen(str2);
    kprintf("    => open %s, return fd: %d\n", file2, (fd2 = sys_open(file2, O_CREAT | O_RDWD)));
    if (fd2 != -1){
        kprintf("    => writing \"%s\" to %s, string len: %d, %d char written\n", str2, file2, len, sys_write(fd2, str2, len));
        kprintf("    => closing %s, return value: %d\n", file2, sys_close(fd2));
        kprintf("    => re-open %s, return fd: %d\n", file2, (fd2 = sys_open(file2, O_RDWD)));
        kprintf("    => reading from %s, %d char read: %s\n", file2, sys_read(fd2, buf, len), buf);
        kprintf("    => closing %s, return value: %d\n", file2, sys_close(fd2));
    }


    kprintf("/--------------- %s test done ---------------/\n", __func__);
}



void test_mkdir_rmdir(void){
    kprintf("/--------------- %s test start ---------------/\n", __func__);
    char *fd1 = "/dir1";
    char *fd2 = "/dir2";
    char *subfd1 = "/dir1/a";
    char *subfd2 = "/dir1/b";
    char *subfd3 = "/dir1/c";
    char *subsubfd1 = "/dir1/a/x";

    
    kprintf("=> Creating %s\n", subfd1);
    kprintf("    => %s creating %s!\n", subfd1, sys_mkdir(subfd1) == -1 ? "fail" : "success");

    kprintf("=> Creating %s\n", fd1);
    kprintf("    => %s creating %s!\n", fd1, sys_mkdir(fd1) == -1 ? "fail" : "success");

    kprintf("=> Re-creating %s\n", subfd1);
    kprintf("    => %s creating %s!\n", subfd1, sys_mkdir(subfd1) == -1 ? "fail" : "success");

    kprintf("=> Creating %s\n", subfd2);
    kprintf("    => %s creating %s!\n", subfd2, sys_mkdir(subfd2) == -1 ? "fail" : "success");

    kprintf("=> Creating %s\n", subfd3);
    kprintf("    => %s creating %s!\n", subfd3, sys_mkdir(subfd3) == -1 ? "fail" : "success");

    kprintf("=> Creating %s\n", fd2);
    kprintf("    => %s creating %s!\n", fd2, sys_mkdir(fd2) == -1 ? "fail" : "success");

    kprintf("=> Creating %s\n", subsubfd1);
    kprintf("    => %s creating %s!\n", subsubfd1, sys_mkdir(subsubfd1) == -1 ? "fail" : "success");

    kprintf("=> Removing %s\n", subfd2);
    kprintf("    => %s removing %s!\n", subfd2, sys_rmdir(subfd2) == -1 ? "fail" : "success");


    kprintf("/--------------- %s test done ---------------/\n", __func__);
}


void test_open_read_close_dir(void){
    kprintf("/--------------- %s test start ---------------/\n", __func__);
    char *fd1 = "/dir1";
    char *fd2 = "/dir2";
    char *subfd1 = "/dir1/a";
    char *subfd2 = "/dir1/b";
    char *subfd3 = "/dir1/c";
    char *subsubfd1 = "/dir1/a/x";

    char *file1 = "/file1";
    char *file2 = "/dir1/file2";
    char *file3 = "/file3";             // empty file

    char *str1 = "Catch me if you can!";
    char *str2 = "Gotcha!";

    kprintf("=> Creating folders:\n");
    kprintf("    => %s create %s!\n", fd1, sys_mkdir(fd1) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", fd2, sys_mkdir(fd2) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd1, sys_mkdir(subfd1) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd2, sys_mkdir(subfd2) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd3, sys_mkdir(subfd3) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subsubfd1, sys_mkdir(subsubfd1) == -1 ? "fail" : "success");

    kprintf("=> Creating and writing files:\n");
    int fd;
    if ((fd = sys_open(file1, O_CREAT | O_RDWD)) == -1){
        kprintf("    => %s create failed!\n", file1);
    } else {
        kprintf("    => %s create success! Writing \"%s\" into it\n", file1, str1);
        sys_write(fd, str1, strlen(str1));
        sys_close(fd);
    }

    if ((fd = sys_open(file2, O_CREAT | O_RDWD)) == -1){
        kprintf("    => %s create failed!\n", file2);
    } else {
        kprintf("    => %s create success! Writing \"%s\" into it\n", file2, str2);
        sys_write(fd, str2, strlen(str2));
        sys_close(fd);
    }

    if ((fd = sys_open(file3, O_CREAT | O_RDWD)) == -1){
        kprintf("    => %s create failed!\n", file3);
    } else {
        kprintf("    => %s create success! Writing noting into it\n", file3);
        sys_close(fd);
    }


    dir_t *dir;
    char *dirs[] = {
        // "/", fd1, subfd1, fd2
        "/"
    };
    int nums = 1;
    for (int i = 0; i < nums; i++){
        kprintf("Content of \"%s\" :\n", dirs[i]);
        // open dir
        if ((dir = sys_opendir(dirs[i])) != NULL){
            char *type = NULL;
            dir_entry_t *de = NULL;
            // read dir
            while ((de = sys_readdir(dir))){
                if (de->f_type == FT_REGULAR)
                    type = "regular";
                else
                    type = "directory";
                kprintf("    %s    %s\n", type, de->filename);
            }
            // close dir
            if (sys_closedir(dir) == 0)
                kprintf("%s close done!\n", dirs[i]);
            else
                kprintf("%s close fail!\n", dirs[i]);
        } else 
            kprintf("%s open fail\n", dir[i]);
    }
    kprintf("/--------------- %s test done ---------------/\n", __func__);
}


void test_getcwd_chdir(void){
    kprintf("/--------------- %s test start ---------------/\n", __func__);
    char *fd1 = "/dir1";
    char *subfd1 = "/dir1/a";

    kprintf("=> Creating folders:\n");
    kprintf("    => %s create %s!\n", fd1, sys_mkdir(fd1) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd1, sys_mkdir(subfd1) == -1 ? "fail" : "success");

    char cwd_buf[32] = {0};
    kprintf("=> Getcwd:\n");
    sys_getcwd(cwd_buf, 32);
    kprintf("    => cwd: %s\n", cwd_buf);

    kprintf("=> Chdir:\n");
    sys_chdir(fd1);
    kprintf("    => change cwd to %s\n", fd1);
    sys_getcwd(cwd_buf, 32);
    kprintf("    => cwd: %s\n", cwd_buf);

    kprintf("=> Chdir:\n");
    sys_chdir(subfd1);
    kprintf("    => change cwd to %s\n", subfd1);
    sys_getcwd(cwd_buf, 32);
    kprintf("    => cwd: %s\n", cwd_buf);

    kprintf("/--------------- %s test done ---------------/\n", __func__);
}



void test_stat(void){
    kprintf("/--------------- %s test start ---------------/\n", __func__);
    char *fd1 = "/dir1";
    char *fd2 = "/dir2";
    char *subfd1 = "/dir1/a";
    char *subfd2 = "/dir1/b";
    char *subfd3 = "/dir1/c";
    char *subsubfd1 = "/dir1/a/x";

    char *file1 = "/file1";
    char *file2 = "/dir1/file2";
    char *file3 = "/file3";             // empty file

    char *str1 = "Catch me if you can!";
    char *str2 = "Gotcha!";

    kprintf("=> Creating folders:\n");
    kprintf("    => %s create %s!\n", fd1, sys_mkdir(fd1) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", fd2, sys_mkdir(fd2) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd1, sys_mkdir(subfd1) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd2, sys_mkdir(subfd2) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subfd3, sys_mkdir(subfd3) == -1 ? "fail" : "success");
    kprintf("    => %s create %s!\n", subsubfd1, sys_mkdir(subsubfd1) == -1 ? "fail" : "success");

    kprintf("=> Creating and writing files:\n");
    int fd;
    if ((fd = sys_open(file1, O_CREAT | O_RDWD)) == -1){
        kprintf("    => %s create failed!\n", file1);
    } else {
        kprintf("    => %s create success! Writing \"%s\" into it\n", file1, str1);
        sys_write(fd, str1, strlen(str1));
        sys_close(fd);
    }

    if ((fd = sys_open(file2, O_CREAT | O_RDWD)) == -1){
        kprintf("    => %s create failed!\n", file2);
    } else {
        kprintf("    => %s create success! Writing \"%s\" into it\n", file2, str2);
        sys_write(fd, str2, strlen(str2));
        sys_close(fd);
    }

    if ((fd = sys_open(file3, O_CREAT | O_RDWD)) == -1){
        kprintf("    => %s create failed!\n", file3);
    } else {
        kprintf("    => %s create success! Writing noting into it\n", file3);
        sys_close(fd);
    }


    dir_t *dir;
    stat_t obj_stat;
    char path[32] = {0};
    path[0] = '/';
    char *p_root = &path[1];
    kprintf("Content of \"/\" :\n");
    // open dir
    if ((dir = sys_opendir("/")) != NULL){
        dir_entry_t *de = NULL;
        // read dir
        while ((de = sys_readdir(dir))){
            // get stat
            strcat(p_root, de->filename);
            sys_stat(path, &obj_stat);
            kprintf(
                "    Info of %s:    inode: %d    size: %d    filetype: %s\n",
                de->filename,
                obj_stat.st_ino,
                obj_stat.st_size,
                obj_stat.st_filetype == FT_DIRECTORY ? "directory" : "regular"
            );
            *p_root = 0;
        }
        // close dir
        if (sys_closedir(dir) == 0)
            kprintf("%s close done!\n", path);
        else
            kprintf("%s close fail!\n", path);
    } else 
        kprintf("%s open fail\n", path);
    kprintf("/--------------- %s test done ---------------/\n", __func__);
}