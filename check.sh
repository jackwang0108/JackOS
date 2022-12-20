#! /bin/bash


if [[ "$1" = "-h" ]] || [ "$1" = "--help" ]; then
    echo "Tools for check user program written to JackOS.img, JackOS-fs.img and user program. Created by Jack Wang"
    echo "Usage: "
    echo "    ./check.sh START_LBA1 START_LBA2 USER_PROGRAM"
    echo "    START_LBA1: start lba in JackOS.img"
    echo "    START_LBA1: start lba in JackOS-fs.img"
    echo "Options:"
    echo "    -h    show this message"
    exit 0
fi


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

shell_folder=$(cd "$(dirname "$0")" || exit; pwd)
file_size=$(ls -l "${shell_folder}/${3}" | awk '{print $5}')
fz=$(echo "obase=8;ibase=10;${file_size}" | bc)
echo "User program:" "$(green "${3}")" ", file size:" "$(purple "${file_size}")" "(decimal) |" "$(purple "${fz}")" "(octal)"


# Location
os="${shell_folder}/JackOS.img"
fs="${shell_folder}/JackOS-fs.img"
up="${shell_folder}/${3}"

echo "Check" "$(green "User program"):" "$up, " "writing to" "$(green "check.uprog")"
off=0; xxd -a -g 4 -s $((off * 512)) -l "${fz}" "${up}" | awk '{print $2, $3, $4, $5, $6}' > check.uprog


echo "Check" "$(green "Kernel image"):" "$os, " "writing to" "$(green "check.os")"
off=$1; xxd -a -g 4 -s $((off * 512)) -l "${fz}" "${os}" | awk '{print $2, $3, $4, $5, $6}' > check.os


echo "Check" "$(green "File System image"):" "$fs, " "writing to" "$(green "check.fs")"
off=$2; xxd -a -g 4 -s $((off * 512)) -l "${fz}" "${fs}" | awk '{print $2, $3, $4, $5, $6}' > check.fs


read -r -p "Run \`code -d check.os check.uprog\`? <Enter/n>: " c
if [[ $c = "n" ]]; then
    echo "Exiting..."
    exit 0
fi
code -d "${shell_folder}/check.os" "${shell_folder}/check.uprog"