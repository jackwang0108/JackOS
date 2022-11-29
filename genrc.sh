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

# get disk image
shell_folder=$(cd "$(dirname "$0")" || exit; pwd)
cd "$shell_folder" || exit
bin_file=$(ls | grep 'img$')
if [[ -z $bin_file ]]; then
    echo "Binary file not detected, creating JackOS.img:"
    bin_file="JackOS.img"
    echo "Generating $bin_file..."
    bximage -q -func=create -hd=60 -imgmode=flat "$shell_folder"/$bin_file
    bin_info=($(bximage -func=info $bin_file -q | awk '/geometry/{print $3}' | awk -F '/' '{print $1,$2,$3}'))
    echo "Binary file created: $bin_file, C/H/S: ${bin_info[0]}/${bin_info[1]}/${bin_info[2]}"
else
    bin_info=($(bximage -func=info $bin_file -q | awk '/geometry/{print $3}' | awk -F '/' '{print $1,$2,$3}'))
    echo "Binary file detected: $bin_file, C/H/S: ${bin_info[0]}/${bin_info[1]}/${bin_info[2]}"
fi


if [[ $1 = "-g" || $1 = "--gdb" || $1 = "gdb" ]]; then
    echo "Using $os config with GDB, writing config to bochsrc"
    echo "PATH=$bin_file, C/H/S=${bin_info[0]}/${bin_info[1]}/${bin_info[2]}"
    sed \
        -e "s/# $os: //g"\
        -e "s/# GDB: //g"\
        -e "s/PATH/$bin_file/g"\
        -e "s/CYLINDERS/${bin_info[0]}/g"\
        -e "s/HEADS/${bin_info[1]}/g"\
        -e "s/SPT/${bin_info[2]}/g"\
        "$shell_folder"/bochsrc.source  > "$shell_folder"/bochsrc
    echo 'Run run `bochs-gdb -f bochsrc` to start bochs'
else
    echo "Using $os config without GDB, writing config to bochsrc"
    echo "PATH=$bin_file, C/H/S=${bin_info[0]}/${bin_info[1]}/${bin_info[2]}"
    sed \
        -e "s/# $os: //g"\
        -e "s/PATH/$bin_file/g"\
        -e "s/CYLINDERS/${bin_info[0]}/g"\
        -e "s/HEADS/${bin_info[1]}/g"\
        -e "s/SPT/${bin_info[2]}/g"\
        "$shell_folder"/bochsrc.source  > "$shell_folder"/bochsrc
    echo 'Run `bochs -f bochsrc` to start bochs'
fi
