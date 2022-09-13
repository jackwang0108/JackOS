![JackOS](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/JackOSALL.png)

# JackOS

A simple operating system kernel written by Shihong(Jack) Wang, for study and practice OS only.

This repository aims to realize a simple OS kernel that provides basic functionalities like scheduling, virtual memory management (segmentation & paging), storage management (file system) and device management (keyboard & mouse).

Since, I'm currently a short term exchange student majoring in Computer Science in UW-Madison, I hope I can finish major functionalities of the kernel while I'm studying in UW-Madison.

Plus, the develop journal will be updated on my [website  episode](http://jackwang.cafe/categories/JackOS%E5%BC%80%E5%8F%91%E6%97%A5%E8%AE%B0/). Forgive me if the posts are in Chinese, I'll try my best to convey myself clearly in English, but not assured.



Read the project README in other language: [简体中文](README-zh.md)



# Workflow Tools (With explanation)

To write, test and debug the kernel, a series of tools are needed. All the tools introduced below are organized according to develop workflow. 

SO, read this section, you will get to know develop workflow of kernel programming, testing and debugging.



## 1. Disk Image

### A. Explanation

A disk is a **physical** block of cells (bits), each cell (bit) can store a bit number, either 0/1.

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





## 2. Programming

Whatever editor/IDE you'd like, `vscode`, `vim`, `lunarvim`, `clion`, etc. is fine. Just you like it.



## 3. Virtual Machines

We need a virtual machine to run and debug our kernel.

### A. Debugging

Under many circumstances, we need to single step debugging. For example, when programming for scheduler or multiprocess, we need single step debugging to see what happened during the seconds. So, for single step debugging, we will use `bochs` .

Popular debugging virtual machines include `bochs` and `qemu`. But since I'm much more familiar with `bochs`, I'll just keep using `bochs`.

You can just install `bochs` via its `sourceforge`, it's very easy to compile and install it. Remember, since the `bochs` can simulate your devices, YOU NEED TO COMPILE IT ON YOUR PLATFORM (YOUR OWN COMPUTER).



### B. Running

To run the kernel, we can use `VirtualBox`. Compared to `VMWare`, `VirtualBox` is an open source virtual machine manager.





# Acknowledgement

**Resources (blog / video / paper / book, etc.) and people listed below help a lot to this project. Thanks to their time, energy and selfless dedication of sharing their knowledge.**

Here's the acknowledgement list:

- 
