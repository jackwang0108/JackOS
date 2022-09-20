#! /bin/bash


shell_folder=$(cd $(dirname "$0");pwd)
echo "Working directory $shell_folder"
cd "$shell_folder" || exit 1

# build
echo "Build JackOS.bin..."
if ! nasm "$shell_folder"/boot/mbr.asm -I "$shell_folder"/boot/include -f bin -o "$shell_folder"/JackOS.bin -l "$shell_folder"/JackOS.lst ;
then
    echo "Build fail!"
fi

# write disk img
echo "Write disk img..."
if ! dd if="$shell_folder"/JackOS.bin of="$shell_folder"/JackOS.img conv=notrunc > /dev/null;
then
    echo "Write fail!"
fi

# run
if ! bochs -f "$shell_folder"/bochsrc; then
    if [ -e "$shell_folder"/JackOS.img.lock ]; then
        rm "$shell_folder"/JackOS.img.lock
    fi
fi

# exit
exit 0
