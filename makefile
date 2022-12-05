BUILD_DIR = ./build
ENTRY_POINT = 0xC0001500
PREFIX = /home/jack/projects/ElephantBook/tools/bin
AS = nasm
CC = $(PREFIX)/i686-elf-gcc
LD = $(PREFIX)/i686-elf-ld
LIB = -I lib/ -I lib/kernel -I lib/user -I kernel -I device -I thread -I userprog
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
		$(BUILD_DIR)/stdio.o


############################################################
##################### 编译C语言代码 ##########################
############################################################

$(BUILD_DIR)/main.o: kernel/main.c \
		lib/kernel/print.h lib/stdint.h kernel/init.h thread/thread.h
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h\
		lib/kernel/print.h lib/stdint.h kernel/interrupt.h device/timer.h\
		kernel/memory.h device/console.h
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


############################################################
###################### 编译汇编文件 ##########################
############################################################

ASFLAGS = -f elf
$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o: lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@


ASLIB = -I boot/include
$(BUILD_DIR)/loader.bin: boot/loader.S
	$(AS) -f bin $(ASLIB) $< -o $@

$(BUILD_DIR)/mbr.bin: boot/mbr.S
	$(AS) -f bin $(ASLIB) $< -o $@


.PHONY: mk_dir hd clean all rc

mk_dir:
	if [ ! -d $(BUILD_DIR) ];then mkdir -p $(BUILD_DIR); fi

bin_folder=$(BUILD_DIR)/..
hd:
	dd if=/dev/zero of=$(bin_folder)/JackOS.img \
		bs=512 seek=0 conv=notrunc count=100000
	dd if=$(BUILD_DIR)/mbr.bin of=$(bin_folder)/JackOS.img \
		bs=512 seek=0 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$(bin_folder)/JackOS.img \
		bs=512 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(bin_folder)/JackOS.img \
		bs=512 seek=9 conv=notrunc

clean:
	cd $(BUILD_DIR) && rm -f ./*
	cd $(bin_folder) && (rm -f JackOS.img || true) && (rm -f JackOS.img.lock || true)

build: $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin

disasm: $(BUILD_DIR)/kernel.bin
	objdump -D -m i386:intel $< > $(BUILD_DIR)/kernel.dump

ll: mk_dir build hd disasm

no-gdb: mk_dir build hd disasm
	bash $(BUILD_DIR)/../genrc.sh

with-gdb: mk_dir build hd disasm
	bash $(BUILD_DIR)/../genrc.sh gdb