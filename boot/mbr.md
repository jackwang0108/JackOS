# `mbr.asm` Explanation

# 1. Explanation

### A. Master Boot Record (MBR)

Hard disk can logically be divided into multiple small blocks. Each block can store small amount of data, usually a block can store 512 (most often), 1024 or 2048 bytes. Each block is called a record. Master Boot Record is a block. The reason why it is called Master is that Master Boot Record is the first block of the disk. That's the reason why MBR is also called start sector. Moreover, each disk has its own MBR.

Although size of the block may vary (512/1024/2048), the most often case and most compatible size is 512. So, in my system, the size of MBR as well as all blocks will be 512 bytes.

### B. BIOS

**The operating system is actually a binary executable program is has nothing different from program on your laptop, like Steam, Chrome, and etc.** The only difference could be your programs run on operating system, but operating system run on bare hardwares. Since your program runs on operating system, os prepares environment for your program. For an example, suppose you are writing a game. And when you want to get current time, you can easily get the time by calling `datetime.datetime.now()` (For Pythoner). And it is the os who actually offer the time.

But now, since we are writing our own os, only thing we get is just a set of hardwares, so, we need to do everything by ourselves like getting time from hardware (we will have a clock who actually do counting staff).

The first thing we need to do is start the system. As mentioned above, os is also a program, but with bare hardware, how can we start the system? Thankfully, we have BIOS and Master Boot Record. BIOS is a program written on a chip. And the chip is on your board. Once powered, the chip will automatically runs the code inside, i.e., once powered, BIOS automatically runs. BIOS will do a lot of things, including checking if the hardware system is fine, whether some vital components like memory is missing and etc.

For us, the most important thing we need to remember about BIOS is that BIOS will check each hard disk we have, and if a disk stores an operating system, then BIOS will load the data in the MBR of that disk into memory.

After the data is loaded into memory, the last instructions of BIOS is jump to the location where data is stored.

Here we may have a lot of questions.

- First, a disk is very large. Nowadays, a disk can easily reach TB size, so how does BIOS know there is an operating system in the disk since search the whole disk costs too much time? **The answer is using flags to help the BIOS**. We using some bits in the MBR to tell the BIOS: "Hi, there is an operating system in this disk (**This disk is bootable**) !". Take a look at the start sector of a real disk (Using tools detailed in README).

  First 512 bytes ends at 200 in hex. And **the flag is 4 byte before ends**. As you can see, the flag of a bootable disk is `55aa`

  ![image-20220920000804548](https://jack-1307599355.cos.ap-shanghai.myqcloud.com/image-20220920000804548.png)
- Second, since BIOS knows which disk has operating system, why it just read the start sector instead of read the system directly so we can start the system know? Well, the system is actually pretty large, it is not feasible to start the system directly using BIOS. Remember, BIOS is just a tiny program on chip. We don't expect BIOS to do much things. Besides, system usually has some prerequisite to start. As system varies, prerequisite varies. So, rather burden BIOS, it's better for system developer to write their own code to prepare for their system and place the codes in start sector so that BIOS can easily start every kind of systems.

### C. BIOS functions

Before program like Steam start, operating system starts first and offer functionalities for program. Similarly, before operating system starts, BIOS starts first. So, luckily, BIOS also provide some basic functionalities for us. Using these functions, we can achieve many basic functions.

All BIOS functions is call via `int 10h`. `int 10h` is a soft interrupt. But we are not going further for the concept of interrupt.

To call different functions, we need to use `ah` register to point out which function we will use.

### D. `0x7c00`

BIOS will read start sector into memory. Yes, it is. But where will start sector data be in the memory?

As you guess, it is `0x7c00`. After BIOS detect a disk is bootable, BIOS will just read the start sector to `0x7c00`.

So, if we write codes (No one would write pure data right? Or, your laptop is stuck since after BIOS jumps, no executable codes then.) and do jumps stuffs, we need to add `0x7c00` as offset, or we will just jump to the very beginning of memory and no one know what it is, data? instructions? Who knows.

So, we use `vstart=0x7c00` to make the `nasm` compiler compiles codes whose address starts from `0x7c00`.
