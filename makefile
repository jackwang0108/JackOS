BUILD_DIR = ./build
ENTRY_POINT = 0xC0001500
PREFIX = /home/jack/projects/JackOS/tools/bin

AS = nasm
CC = $(PREFIX)/i686-elf-gcc
LD = $(PREFIX)/i686-elf-ld
OBJDUMP = $(PREFIX)/i686-elf-objdump

LIB = -I lib/ -I lib/kernel -I lib/user -I kernel -I device -I thread -I userprog -I fs -I shell
# -W 表示Warning相关的Flag, -f 表示选择option, gcc为了加速会对一些诸如abs，strncpy等进行重定义，禁止gcc的这一行为
CFLAGS = -O0 -W -Wall $(LIB) -c -fno-builtin -Werror=strict-prototypes -Wmissing-prototypes -g -Werror=incompatible-pointer-types
LDFLAGS = -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o\
		$(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o\
		$(BUILD_DIR)/debug.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o\
		$(BUILD_DIR)/string.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o\
		$(BUILD_DIR)/switch.o $(BUILD_DIR)/console.o $(BUILD_DIR)/sync.o\
		$(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o\
		$(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall-init.o\
		$(BUILD_DIR)/stdio.o $(BUILD_DIR)/kstdio.o $(BUILD_DIR)/ide.o\
		$(BUILD_DIR)/dir.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o\
		$(BUILD_DIR)/super_block.o $(BUILD_DIR)/file.o $(BUILD_DIR)/test.o\
		$(BUILD_DIR)/fork.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/builtin_cmd.o\
		$(BUILD_DIR)/exec.o $(BUILD_DIR)/assert.o $(BUILD_DIR)/wait_exit.o\
		$(BUILD_DIR)/pipe.o


############################################################
###################### 编译内核C语言代码 ######################
############################################################

$(BUILD_DIR)/main.o: kernel/main.c \
		lib/kernel/print.h lib/stdint.h kernel/init.h thread/thread.h\
		shell/shell.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h\
		lib/kernel/print.h lib/stdint.h kernel/interrupt.h \
		device/timer.h device/console.h device/keyboard.h device/ide.h\
		thread/thread.h kernel/memory.h\
		userprog/tss.h userprog/syscall-init.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h\
		lib/kernel/print.h lib/stdint.h kernel/global.h kernel/io.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c device/timer.h\
		lib/stdint.h kernel/io.h lib/kernel/print.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h\
		kernel/io.h	lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h\
		lib/stdint.h lib/kernel/print.h lib/string.h kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h\
		lib/stdint.h lib/kernel/print.h lib/string.h kernel/debug.h	
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/string.c lib/string.h\
		lib/stdint.h kernel/global.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h\
		lib/stdint.h kernel/global.h kernel/memory.h lib/string.h\
		lib/kernel/list.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h\
		kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c device/console.h\
		lib/stdint.h lib/kernel/print.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c thread/sync.h\
		lib/kernel/list.h lib/stdint.h thread/thread.h kernel/interrupt.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c device/keyboard.h\
		lib/kernel/print.h kernel/global.h kernel/interrupt.h\
		kernel/io.h device/ioqueue.c
	$(CC) $(CFLAGS) $< -o $@
		
$(BUILD_DIR)/ioqueue.o: device/ioqueue.c device/ioqueue.h\
		lib/stdint.h thread/thread.h thread/sync.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o: userprog/tss.c userprog/tss.h\
		kernel/global.h thread/thread.h lib/kernel/print.h lib/string.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: userprog/process.c userprog/process.h\
		kernel/global.h thread/thread.h lib/string.h kernel/interrupt.h\
		device/console.h userprog/tss.h kernel/debug.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: lib/user/syscall.c lib/user/syscall.h\
		lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall-init.o: userprog/syscall-init.c userprog/syscall-init.h\
		lib/stdint.h thread/thread.h lib/kernel/print.h lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h\
		lib/stdint.h kernel/global.h lib/string.h\
		lib/user/syscall.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/kstdio.o: lib/kernel/kstdio.c lib/kernel/kstdio.h\
		lib/stdio.h device/console.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c device/ide.h\
		lib/stdint.h lib/kernel/list.h lib/kernel/bitmap.h\
		device/timer.h thread/sync.h\
		lib/stdio.h lib/kernel/kstdio.h\
		kernel/io.h kernel/debug.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o: fs/dir.c fs/dir.h\
		fs/inode.h fs/fs.h lib/stdint.h device/ide.h 
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fs.o: fs/fs.c fs/fs.h\
		device/ide.h fs/inode.h kernel/debug.h lib/string.h lib/kernel/kstdio.h fs/dir.h lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/inode.o: fs/inode.c fs/inode.h\
		lib/kernel/list.h lib/stdint.h fs/fs.h  device/ide.h kernel/debug.h lib/string.h kernel/interrupt.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/super_block.o: fs/super_block.c fs/super_block.h\
		kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/file.o: fs/file.c fs/file.h\
		fs/inode.h lib/stdint.h kernel/debug.h thread/thread.h\
		lib/string.h lib/kernel/kstdio.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/test.o: kernel/test.c kernel/test.h\
		fs/fs.h device/ide.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/assert.o: lib/user/assert.c lib/user/assert.h\
		lib/stdio.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o: userprog/fork.c userprog/fork.h\
		lib/stdint.h kernel/global.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/shell.o: shell/shell.c shell/shell.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/builtin_cmd.o: shell/builtin_cmd.c shell/builtin_cmd.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/exec.o: userprog/exec.c userprog/exec.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/wait_exit.o: userprog/wait_exit.c userprog/wait_exit.o\
		lib/stdint.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/pipe.o: shell/pipe.c shell/pipe.h\
		lib/stdint.h
	$(CC) $(CFLAGS) $< -o $@


############################################################
##################### 编译内核汇编代码 ########################
############################################################

ASFLAGS = -f elf
$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@


############################################################
######################## 链接内核 ###########################
############################################################

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@



############################################################
################# 编译MBR和LOADER汇编代码 ####################
############################################################

ASLIB = -I boot/include
$(BUILD_DIR)/loader.bin: boot/loader.S
	$(AS) -f bin $(ASLIB) $< -o $@

$(BUILD_DIR)/mbr.bin: boot/mbr.S
	$(AS) -f bin $(ASLIB) $< -o $@


############################################################
######################## 编译CRT ###########################
############################################################

CRT = $(BUILD_DIR)/crt.a
CRT_LIB = \
		$(BUILD_DIR)/string.o			\
		$(BUILD_DIR)/syscall.o			\
		$(BUILD_DIR)/stdio.o			\
		$(BUILD_DIR)/assert.o

AR = $(PREFIX)/i686-elf-ar

$(BUILD_DIR)/start.o: command/start.S
	$(AS) -f elf $< -o $@

$(CRT): $(CRT_LIB) $(BUILD_DIR)/start.o
	$(AR) rcs $@ $(CRT_LIB) $(BUILD_DIR)/start.o

############################################################
###################### 编译用户程序 ##########################
############################################################

U_LIB = -I lib/ -I lib/user -I fs/ -I lib/kernel -I kernel
# -W 表示Warning相关的Flag, -f 表示选择option, gcc为了加速会对一些诸如abs，strncpy等进行重定义，禁止gcc的这一行为
U_CFLAGS = -W -Wall $(U_LIB) -c -fno-builtin -Werror=strict-prototypes -Wmissing-prototypes -Werror=incompatible-pointer-types -Wsystem-headers


$(BUILD_DIR)/_prog_no_arg.o: command/prog_no_arg.c\
		lib/stdio.h lib/user/syscall.h
	$(CC) $(U_CFLAGS) $< -o $@

$(BUILD_DIR)/_prog_with_arg.o: command/prog_with_arg.c\
		lib/stdio.h lib/user/syscall.h
	$(CC) $(U_CFLAGS) $< -o $@

$(BUILD_DIR)/_prog_pipe.o: command/prog_pipe.c\
		lib/stdio.h lib/user/syscall.h lib/string.h lib/stdint.h
	$(CC) $(U_CFLAGS) $< -o $@

$(BUILD_DIR)/_cat.o: command/cat.c\
		lib/stdio.h lib/user/syscall.h lib/string.h
	$(CC) $(U_CFLAGS) $< -o $@

$(BUILD_DIR)/_touch.o: command/touch.c\
		lib/stdio.h lib/user/syscall.h lib/string.h lib/stdint.h
	$(CC) $(U_CFLAGS) $< -o $@

$(BUILD_DIR)/_echo.o: command/echo.c\
		lib/stdio.h lib/user/syscall.h lib/string.h lib/stdint.h
	$(CC) $(U_CFLAGS) $< -o $@

############################################################
###################### 链接用户程序 ##########################
############################################################

$(BUILD_DIR)/_prog_no_arg: $(BUILD_DIR)/_prog_no_arg.o\
		$(CRT)
	$(LD) $< $(CRT) -o $@

$(BUILD_DIR)/_prog_with_arg: $(BUILD_DIR)/_prog_with_arg.o\
		$(CRT)
	$(LD) $< $(CRT) -o $@

$(BUILD_DIR)/_prog_pipe: $(BUILD_DIR)/_prog_pipe.o\
		$(CRT)
	$(LD) $< $(CRT) -o $@

$(BUILD_DIR)/_cat: $(BUILD_DIR)/_cat.o\
		$(CRT)
	$(LD) $< $(CRT) -o $@

$(BUILD_DIR)/_touch: $(BUILD_DIR)/_touch.o\
		$(CRT)
	$(LD) $< $(CRT) -o $@

$(BUILD_DIR)/_echo: $(BUILD_DIR)/_echo.o\
		$(CRT)
	$(LD) $< $(CRT) -o $@

############################################################
###################### 命令行伪目标 ##########################
############################################################

.PHONY: mk_dir hd clean-os clean-fs clean-build all rc

mk_dir:
	if [ ! -d $(BUILD_DIR)/dumps ];then mkdir -p $(BUILD_DIR)/dumps; fi

bin_folder=$(BUILD_DIR)/..
hd:
	bash $(BUILD_DIR)/../genrc.sh
	dd if=$(BUILD_DIR)/mbr.bin of=$(bin_folder)/JackOS.img \
		bs=512 seek=0 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$(bin_folder)/JackOS.img \
		bs=512 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(bin_folder)/JackOS.img \
		bs=512 seek=9 conv=notrunc

write_u_prog: \
			$(BUILD_DIR)/_prog_no_arg \
			$(BUILD_DIR)/_prog_with_arg \
			$(BUILD_DIR)/_prog_pipe\
			$(BUILD_DIR)/_cat\
			$(BUILD_DIR)/_touch\
			$(BUILD_DIR)/_echo

	@echo "Size of $(BUILD_DIR)/_prog_no_arg: " $(shell ls -l $(BUILD_DIR)/_prog_no_arg | awk '{print $$5}') " bytes"
	dd  if=$(BUILD_DIR)/_prog_no_arg of=$(bin_folder)/JackOS.img \
		count=$(shell ls -l $(BUILD_DIR)/_prog_no_arg | awk '{printf("%d", ($$5+511)/512)}') bs=512 seek=30000 conv=notrunc

	@echo "Size of $(BUILD_DIR)/_prog_with_arg: " $(shell ls -l $(BUILD_DIR)/_prog_with_arg | awk '{print $$5}') " bytes"
	dd  if=$(BUILD_DIR)/_prog_with_arg of=$(bin_folder)/JackOS.img \
		count=$(shell ls -l $(BUILD_DIR)/_prog_with_arg | awk '{printf("%d", ($$5+511)/512)}') bs=512 seek=35000 conv=notrunc

	@echo "Size of $(BUILD_DIR)/_prog_pipe: " $(shell ls -l $(BUILD_DIR)/_prog_pipe | awk '{print $$5}') " bytes"
	dd  if=$(BUILD_DIR)/_prog_pipe of=$(bin_folder)/JackOS.img \
		count=$(shell ls -l $(BUILD_DIR)/_prog_pipe | awk '{printf("%d", ($$5+511)/512)}') bs=512 seek=45000 conv=notrunc

	@echo "Size of $(BUILD_DIR)/_cat: " $(shell ls -l $(BUILD_DIR)/_cat | awk '{print $$5}') " bytes"
	dd  if=$(BUILD_DIR)/_cat of=$(bin_folder)/JackOS.img \
		count=$(shell ls -l $(BUILD_DIR)/_cat | awk '{printf("%d", ($$5+511)/512)}') bs=512 seek=40000 conv=notrunc

	@echo "Size of $(BUILD_DIR)/_touch: " $(shell ls -l $(BUILD_DIR)/_touch | awk '{print $$5}') " bytes"
	dd  if=$(BUILD_DIR)/_touch of=$(bin_folder)/JackOS.img \
		count=$(shell ls -l $(BUILD_DIR)/_touch | awk '{printf("%d", ($$5+511)/512)}') bs=512 seek=50000 conv=notrunc

	@echo "Size of $(BUILD_DIR)/_echo: " $(shell ls -l $(BUILD_DIR)/_echo | awk '{print $$5}') " bytes"
	dd  if=$(BUILD_DIR)/_echo of=$(bin_folder)/JackOS.img \
		count=$(shell ls -l $(BUILD_DIR)/_echo | awk '{printf("%d", ($$5+511)/512)}') bs=512 seek=55000 conv=notrunc

clean-os:
	cd $(bin_folder) && (rm -f JackOS.img || true) && (rm -f JackOS.img.lock || true)

clean-fs:
	cd $(bin_folder) && (rm -f JackOS-fs.img || true) && (rm -f JackOS-fs.img.lock || true)

clean-build:
	cd $(BUILD_DIR) && rm -rf ./*

clean: clean-build clean-os clean-fs

kernel: $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin

user: $(BUILD_DIR)/_prog_no_arg

disasm: $(BUILD_DIR)/kernel.bin\
		$(BUILD_DIR)/_prog_no_arg \
		$(BUILD_DIR)/_prog_with_arg \
		$(BUILD_DIR)/_prog_pipe\
		$(BUILD_DIR)/_cat\
		$(BUILD_DIR)/_touch\
		$(BUILD_DIR)/_echo
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/kernel.bin > $(BUILD_DIR)/dumps/kernel.dump
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/_prog_no_arg > $(BUILD_DIR)/dumps/_prog_no_arg.dump
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/_prog_with_arg > $(BUILD_DIR)/dumps/_progwith_arg.dump
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/_prog_pipe > $(BUILD_DIR)/dumps/_prog_pipe.dump
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/_cat > $(BUILD_DIR)/dumps/_cat.dump
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/_touch > $(BUILD_DIR)/dumps/_touch.dump
	$(OBJDUMP) -D -M intel:i386 $(BUILD_DIR)/_echo > $(BUILD_DIR)/dumps/_echo.dump


ll: mk_dir kernel hd disasm

no-gdb: mk_dir kernel hd disasm user write_u_prog
	bash $(BUILD_DIR)/../genrc.sh

with-gdb: mk_dir kernel hd disasm user write_u_prog
	bash $(BUILD_DIR)/../genrc.sh -g

run-no-gdb: no-gdb
	bochs -f $(BUILD_DIR)/../bochsrc

run-with-gdb: with-gdb
	bochs -f $(BUILD_DIR)/../bochsrc