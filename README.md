![JackOS](.assets/JackOSALL.png)

# JackOS

A simple operating system kernel written by Shihong(Jack) Wang, for study and practice OS only.

This repository aims to realize a simple OS kernel that provides basic functionalities like scheduling, virtual memory management (segmentation & paging), storage management (file system) and device management (keyboard & mouse).

Since, I'm currently a short term exchange student majoring in Computer Science in UW-Madison, I hope I can finish major functionalities of the kernel while I'm studying in UW-Madison.

Plus, the develop journal will be updated on my [website  episode](http://jackwang.cafe/categories/JackOS%E5%BC%80%E5%8F%91%E6%97%A5%E8%AE%B0/). Forgive me if the posts are in Chinese, I'll try my best to convey myself clearly in English, but not assured.



Read the project README in other language: [简体中文](README-zh.md)



# Usage

## preparation

`init.sh` is a simple command line tool for initializing and quickly running the JackOS.

> Note: by default, cross-compile toolchain source code will be download at `<ProjectBaseDir>/tools/src`, and compiled cross-compile tools will be installed at `<ProjectBaseDir>/tools/bin`. You can change first few lines of `init.sh` to change installation path.

Run `bash init.sh -h`, and you will see:

```sh
❯ bash init.sh -h
Init tools for downloading, compiling, installing debugger and corss-compile toolchain of JackOS, created by Jack Wang
Options:
    -h, --help                Show this help message
    -d, --download            Download toolchain
    -c, --compile             Compile toolchain
```

To run the OS, first run:

```sh
bash init.sh -d
```

The tool will download cross-compile toolchains for you. **If download failed, remove the download folder by `rm -r <download-folder>`  and re-run `bash init.sh -d` to download.


After that, run following command to compile and install cross-compile toolchains:

```sh
bash init.sh -c
```

This may take a few minutes. 


# Target

What I want JackOS be like are:

- a 32-bit OS working under text mode (I don't want to **currently** burden myself with graphic/GPU things...)
- use paging to manage physical memory and provide virtual memory management.
- has a basic `ext` file system, which could offer abstraction like `file`/`path` and etc.
- can management basic device like keyboard / flash.
- may be deadlock detection? Like banker's algorithm. I don't know, just probably.



# Workflow Tools (With explanation)

To write, test and debug the kernel, a series of tools are needed. All the tools introduced below are organized according to develop workflow. 

SO, read this section, you will get to know develop workflow of kernel programming, testing and debugging.



## 1. Disk Image

### A. Explanation

A disk is a **physical** collections of cells/block (bits), each cell (bit) can store a bit number, either 0/1.

So, we can divide disk into two parts: a series of binary codes and the physical device that store the binary codes.

![Image of my disk](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913104104400.png)

From the perspective of binary code, files are also collections of binary codes. So, we can store the binary codes in the disk into a files.

![File is also collection of binary codes](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913104550372.png)

So, **a file that identically stores the binary codes of a disk is called disk image**. And **using disk image, we can simulate a hard disk**.

> Notes: **disk image is a file**. In essence, disk image has no difference to your `.ppt` , `.xlsx` files. Only the content in the disk image decide if the file is a disk image. If the binary content of a file matches a certain disk, then the file is a disk image of that disk.

### B. Create Disk Image

To create disk image, several tools are available.

#### 1) dd

`dd` is a terminal tool which help us to **edit** binary files. Here we use it to create a disk image.

PS: edit includes more than create, we will cover that shortly.

```shell
dd if=/dev/zero of=YourImageName.img bs=BlockSize count=BlockNum
```

`/dev/zero` is a special device file which continuously produces zeros. Here we select input file (`if`) whose binary codes are all zero and output to output file (`of`)  with given (`count` ) number of blocks. Each block is `bs`  bytes.

An example is 

```shell
dd if=/dev/zero of=disk.img bs=512 count=3
```

where we create a disk image `disk.img` of 3 (blocks) \* 512 (bytes each) = 1.5M bytes.

![disk.img is full of zero](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913134447010.png)



#### 2) bximage

`bximage` is a disk image tool within `bochs`. `bochs` is a virtual machine manager with support single step debugging. So we will use `bochs` to debug our kernel. 

When compiling and installing `bochs`, `bximage` is also installed, so just use the `bximage` tool.

![bximage create disk image](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913135032077.png)



### C. View Disk Image

Since our kernel is stored on the disk, and the first step to start the system is to read the system on the disk, so we need to directly read binary bits of the disk image in many occasions. 

Here are tools that can help us to read binary bits of a disk image (not only disk image, but any files/device).

#### 1) wxHexEditor

`winHex` is a very convenient tool in `Windows`. Unfortunately, `winHex` doesn't support `Linux`, so we got `wxHexoEditor`, the `Linux` third party open source clone of `winHex`.

Simply install `wxHexEditor` via `apt`, and use `wxHexEditor` directly in terminal

```shell
wxHexEditor
```

You will see

![wxHexEditor](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913135728375.png)

You can open a file / device through menu on the top.

PS: **Open a device need root privilege**



#### 2) hexdump

Sometimes, we just want to get a quick glance at the binary files in the terminal. So, `hexdump` is a simple tool to help us.

Basic usage

```shell
hexdump -C -n length -s skip file_name
```

Here, `-C` is format option. `-C` means showing offset on the left, hex of file content on the middle and ASCII characters on the right.

PS: Empty lines will be folded and represented by `*`

![hexdump](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913140754164.png)



#### 3) vim + xxd

You can use vim and xxd to have a better reading.

```shell
vim <(xxd BinaryFile)
```

For example,

```shell
vim <(xxd test.o)
```

and you will see

PS: `test.o` is a short C++ compiled program, listed below shortly.

![Use vim to view binary files, including disk image](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913151932328.png)

#### 4) vimdiff + xxd

A lot of times, we may need to compare two binary files, to do that, we can combine `vim difference`  with `xxd`

```shell
vimdiff <(xxd BinaryFile1) <(BinaryFile2)
```

For example, compare `test.o`  and `disk.img`

```shell
vimdiff <(xxd test.o) <(xxd disk.img)
```

and you will see

![Use vim to compare two binary files](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913151022228.png)







### D. Edit Disk Image

Since we need to write booting code in the start sector, we need to edit binary bits of disk image directly.



#### 1) dd (Copy/overwrite)

we can use `dd` to write the binary bits of our program into the disk image. Simply change the `if` from `/dev/zero` to our program.

Compile a short program first

```shell
echo -e 'int main(){ int a = 1+1; return 0; }' > test.cpp
g++ test.cpp -o test.o
```

And then we use `dd` to write the code into `disk.img`

```shell
dd if=test.o of=disk.img conv=notrunc 
```

PS: **`conv` means special options, here we use `notrunc`, means do not truncate ·`disk.img` after write `test.o` into `disk.img`.**

Then, we use `vimdiff` to compare `test.o` and `disk.img`

```shell
vimdiff <(xxd test.o) <(xxd disk.img)
```

And you will see

![vimdiff after dd](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913152646300.png)



#### 2) wxHexEditor (Change Bits)

Some times, we just want to change a few bits of the disk image, for example, changing bootable flag, change section number and etc.

So, we can use `wxHexEditor` to change some bits.

![Use wxHexEditor to change 45->49](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913153443486.png)

And result of `vimdiff` is

![Result](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220913153620731.png)





## 2. Programming and Compile

### A. Text Editor

Whatever editor/IDE you'd like, `vscode`, `vim`, `lunarvim`, `clion`, etc. Anything can write is fine. Just you like it.

### B. Compiler

#### 1) Assembly Compiler

**For developing OS, we have to do some dirty assembly works :(**. There's a lot of assembly compiler we can use, like `masm`, `at` and etc. Likewise, we can choose `x86` format assembly or `AT&T` format.

But in consideration of online help, it's better to use `x86` assembly format for it's classic and there are a lot of resources we can use on the Internet.

But unfortunately, one of the most famous and classic `x86` assembler is `masm` which belongs to Intel and **it is not free**. 

So, I just choose to use `nasm`, a new (considering `masm` was created in 1970s), popular (more and more people are using `nasm`), and fancy (it offers a lot of macro) `x86` assembler.



#### 2) C Compiler

Not decided yet, since cross-compile may be needed, so I'm still seeking which C compiler (`gcc`/`clang`) is more easy to cross-compile.



## 3. Virtual Machines

We need a virtual machine to run and debug our kernel.

### A. Bochs (Debugging)

Under many circumstances, we need to single step debugging. For example, when programming for scheduler or multiprocess, we need single step debugging to see what happened during the seconds. So, for single step debugging, we will use `bochs` .

Popular debugging virtual machines include `bochs` and `qemu`. But since I'm much more familiar with `bochs`, I'll just keep using `bochs`.

#### 1) Installation (Bochs 2.7)

You can just install `bochs` via its `sourceforge`, it's very easy to compile and install it. Remember, since the `bochs` can simulate your devices, YOU NEED TO COMPILE IT ON YOUR PLATFORM (YOUR OWN COMPUTER).

We first need to download `bochs`, here we use `bochs 2.7`

```shell
visit https://sourceforge.net/projects/bochs/files/bochs/2.7/bochs-2.7.tar.gz/download to download
```

then extract source code.

```shell
tar xzvf bochs-2.7.tar.gz
```

Before compile, configure compile options first (a popular configure command is listed below):

```shell
cs bochs-2.7
./configure --enable-debugger --enable-iodebug --enable-x86-debugger --with-x --with-x11 
```

Finally, make and install `bochs`

```shell
make -j $(nproc)
make install
```



For more information, please check `bochs`  manual `Chapter 3: Installation`



#### 2) Configure Bochs

To run our kernel, we need to setup configuration of the virtual machine that our kernel will be run on.

Here's my `bochrc` configuration

```shell
###############################################################

# Configuration file for Bochs

###############################################################

# how much memory the emulated machine will have

megs: 32

# filename of ROM images

romimage: file=/usr/local/share/bochs/BIOS-bochs-latest
vgaromimage: file=/usr/local/share/bochs/VGABIOS-lgpl-latest

# what disk images will be used

# floppya: 1_44=a.img, status=inserted

# choose the boot disk.

boot: disk

# where do we send log messages?

# log: bochsout.txt

# disable the mouse and enable us keyboard

mouse: enabled=0
keyboard: keymap=/usr/local/share/bochs/keymaps/x11-pc-us.map

# 硬盘设置

ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14

# Created hard disk image 'JackOS.img' with CHS=406/16/63

ata0-master: type=disk, path="JackOS.img", mode=flat

# gdb远程调试支持, 需要编译Bochs时候指定--enable-gdb-stub参数, 否则报错

# gdbstub: enabled=1, port=1234, text_base=0, data_base=0, bss_base=0
```





#### 3) Basic Debug Command

Manual is always the best tutorial. Please check the manual: https://bochs.sourceforge.io/doc/docbook/user/index.html

For Chinese reader, here's a Chinese version of basic debug command: https://petpwiuta.github.io/2020/05/09/Bochs%E8%B0%83%E8%AF%95%E5%B8%B8%E7%94%A8%E5%91%BD%E4%BB%A4/





### B. Running

To run the kernel, we can use `VirtualBox`. Compared to `VMWare`, `VirtualBox` is an open source virtual machine manager.





# Acknowledgement

**Resources (blog / video / paper / book, etc.) and people listed below help a lot to this project. Thanks to their time, energy and selfless dedication of sharing their knowledge.**

Here's the acknowledgement list:

- 
