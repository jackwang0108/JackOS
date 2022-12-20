#! /bin/bash

# terminal colors
purple='\e[35m'
green='\e[32m'
red='\e[31m'
return='\e[0m'

function red(){
    echo -e "$red$1$return"
}

function green(){
    echo -e "$green$1$return"
}

function purple() {
    echo -e "$purple$1$return"
}

if [[ "$1" = "-h" ]] || [[ "$1" = "--help" ]]; then
    echo "Tools for check user program written to JackOS.img, JackOS-fs.img and user program. Created by Jack Wang"
    echo "Usage: "
    echo "    ./check.sh" "$(green "START_LBA1")" "$(purple START_LBA2)" "USER_PROGRAM"
    echo "     " "$(green "START_LBA1:")" "start lba in JackOS.img"
    echo "     " "$(purple "START_LBA2:")" "start lba in JackOS-fs.img"
    echo "Example: "
    echo "    ./check.sh 30000 2674 build/_prog_no_arg"
    echo "    ./check.sh 35000 2725 build/_prog_no_arg"
    echo "Options:"
    echo "    -h, --help      show this help message"
    echo "    -d, --delete    delete check files"
    exit 0
fi


if [[ $1 = "-d" ]] || [[ $1 = "--delete" ]]; then
    echo "Remove check files:" green check.{os,fs,uprog}
    rm -f check.{os,fs,uprog}
    exit 0
fi


# Location
shell_folder=$(cd "$(dirname "$0")" || exit; pwd)
os="${shell_folder}/JackOS.img"
fs="${shell_folder}/JackOS-fs.img"
up="${shell_folder}/${3}"

if [[ ! -e "${os}" ]] || [[ ! -e "${fs}" ]] || [[ ! -e "${up}" ]]; then
    red "User program or disk images not detected, run \`make TARGET\` first"
    exit 255
fi

file_size=$(ls -l "${shell_folder}/${3}" | awk '{print $5}')
fz=$(echo "obase=8;ibase=10;${file_size}" | bc)
echo "User program:" "$(green "${3}")" ", file size:" "$(purple "${file_size}")" "(decimal) |" "$(purple "${fz}")" "(octal)"


echo "Check" "$(green "User program"):" "$up, " "writing to" "$(green "check.uprog")"
off=0; xxd -a -g 4 -s $((off * 512)) -l "${fz}" "${up}" | awk '{print $2, $3, $4, $5, $6}' > check.uprog


echo "Check" "$(green "Kernel image"):" "$os, " "writing to" "$(green "check.os")"
off=$1; xxd -a -g 4 -s $((off * 512)) -l "${fz}" "${os}" | awk '{print $2, $3, $4, $5, $6}' > check.os


echo "Check" "$(green "File System image"):" "$fs, " "writing to" "$(green "check.fs")"
off=$2; xxd -a -g 4 -s $((off * 512)) -l "${fz}" "${fs}" | awk '{print $2, $3, $4, $5, $6}' > check.fs


read -r -p "Run \`code -d check.os check.uprog\`? <Enter/n>: " c
if [[ $c = "n" ]]; then
    green "You can run \`code -d check.XXX check.XXX manually\`"
    exit 0
fi
code -d "${shell_folder}/check.os" "${shell_folder}/check.uprog"