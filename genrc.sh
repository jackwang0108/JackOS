#! /bin/bash
# check system
sys=$(uname -s)
case $sys in
    Linux)
        os=LINUX
        ;;
    Darwin)
        os=MACOS
        ;;
    FreeBSD)
        os=LINUX
        ;;
    *)
        os=UNKNOWN
        ;;
esac

if [ "$os" == "UNKNOWN" ]; then
    echo "Unknown OS, exit..."
    exit
fi

# check language
keyword='Binary'
field=3
if [ "$LANG" == "zh_CN.UTF-8" ]; then
    keyword='二进制'
    field=2
fi

# get kernel image and file system image
shell_folder=$(cd "$(dirname "$0")" || exit; pwd)
cd "$shell_folder" || exit
kernel_img=$(ls | grep 'img$' | grep -v '\bfs\b')
if [[ -z $kernel_img ]]; then
    echo "Kernel image not detected, creating JackOS.img:"
    kernel_img="JackOS.img"
    echo "Generating $kernel_img..."
    bximage -q -func=create -hd=60 -imgmode=flat "$shell_folder"/$kernel_img
    kbin_info=($(bximage -func=info $kernel_img -q | awk '/geometry/{print $3}' | awk -F '/' '{print $1,$2,$3}'))
    echo "Kernel image created: $kernel_img, C/H/S: ${kbin_info[0]}/${kbin_info[1]}/${kbin_info[2]}"
else
    kbin_info=($(bximage -func=info $kernel_img -q | awk '/geometry/{print $3}' | awk -F '/' '{print $1,$2,$3}'))
    echo "Kernel image detected: $kernel_img, C/H/S: ${kbin_info[0]}/${kbin_info[1]}/${kbin_info[2]}"
fi

fs_img=$(ls | grep 'img$' | grep '\bfs\b')
if [[ -z $fs_img ]]; then
    echo "File system image not detected, please make sure your file system image contaion 'fs', like: xxx-fs-xxx.img"
    echo "Creating JackOS-fs.img:"
    fs_img="JackOS-fs.img"
    echo "Generating $fs_img..."
    bximage -q -func=create -hd=80 -imgmode=flat "$shell_folder"/$fs_img
    fsbin_info=($(bximage -func=info $fs_img -q | awk '/geometry/{print $3}' | awk -F '/' '{print $1,$2,$3}'))
    echo "File system image created: $fs_img, C/H/S: ${fsbin_info[0]}/${fsbin_info[1]}/${fsbin_info[2]}"
    echo "Writing partition info into $fs_img"
	sfdisk $fs_img < "$shell_folder"/JackOS-fs.sfdisk
else
    fsbin_info=($(bximage -func=info $fs_img -q | awk '/geometry/{print $3}' | awk -F '/' '{print $1,$2,$3}'))
    echo "File system image detected: $fs_img, C/H/S: ${fsbin_info[0]}/${fsbin_info[1]}/${fsbin_info[2]}"
    echo "Partition info"
    fdisk -l $fs_img
    echo
    echo "Please check the partition info of $fs_img whether it matches partition info in JackOS-fs.sfdisk"
fi


if [[ $1 = "-g" || $1 = "--gdb" || $1 = "gdb" ]]; then
    echo "Using $os config with GDB, writing config to bochsrc"
    gdb="s/# GDB: //g" 
else
    echo "Using $os config without GDB, writing config to bochsrc"
    gdb=""
fi

echo "KPATH=$kernel_img, C/H/S=${kbin_info[0]}/${kbin_info[1]}/${kbin_info[2]}"
echo "FSPATH=$fs_img, C/H/S=${fsbin_info[0]}/${fsbin_info[1]}/${fsbin_info[2]}"
sed \
    -e "s/# $os: //g"\
    -e "$gdb"\
    -e "s/KPATH/$kernel_img/g"\
    -e "s/KCYLINDERS/${kbin_info[0]}/g"\
    -e "s/KHEADS/${kbin_info[1]}/g"\
    -e "s/KSPT/${kbin_info[2]}/g"\
    -e "s/FSPATH/$fs_img/g"\
    -e "s/FSCYLINDERS/${fsbin_info[0]}/g"\
    -e "s/FSHEADS/${fsbin_info[1]}/g"\
    -e "s/FSSPT/${fsbin_info[2]}/g"\
    "$shell_folder"/bochsrc.source  > "$shell_folder"/bochsrc

if [[ $1 = "-g" || $1 = "--gdb" || $1 = "gdb" ]]; then
    echo 'Run run `bochs-gdb -f bochsrc` to start bochs'
else
    echo 'Run `bochs -f bochsrc` to start bochs'
fi