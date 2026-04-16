#!/bin/bash

# 1. Commit Step
echo "Checking for changes..."
if [ -n "$(git status --porcelain)" ]; then
    echo "Files are modified. If you want to commit, type your message. Otherwise, leave blank to skip:"
    read commit_msg
    if [ -n "$commit_msg" ]; then
        git add .
        git commit -m "$commit_msg"
        git push -u origin main --force
    else
        echo "Skipping commit."
    fi
else
    echo "No changes detected, skipping commit."
fi

# 2. Clean and Build
rm -f *.o kernel.elf aaron_os.iso
echo "Compiling AaronOS..."

nasm -f elf32 boot.s -o boot.o
gcc -m32 -c keyboard.c -o keyboard.o -ffreestanding -O2 -fno-stack-protector
gcc -m32 -c installer.c -o installer.o -ffreestanding -O2 -fno-stack-protector
gcc -m32 -c editor.c -o editor.o -ffreestanding -O2 -fno-stack-protector
gcc -m32 -c fat16.c -o fat16.o -ffreestanding -O2 -fno-stack-protector
gcc -m32 -c memory.c -o memory.o -ffreestanding -O2 -fno-stack-protector
gcc -m32 -c gui.c -o gui.o -ffreestanding -O2 -fno-stack-protector
gcc -m32 -c kernel.c -o kernel.o -ffreestanding -O2 -fno-stack-protector

ld -m elf_i386 -T linker.ld -o kernel.elf boot.o keyboard.o installer.o editor.o fat16.o memory.o gui.o kernel.o --no-warn-rwx-segments

mkdir -p iso_root/boot/grub
cp kernel.elf iso_root/boot/
grub-mkrescue -o aaron_os.iso iso_root

# 3. Draft Release Step
echo "Build successful! Create a GitHub draft release? [y/N]"
read release_choice
if [[ "$release_choice" == [Yy]* ]]; then
    echo "Enter release tag (e.g., v3.9.0):"
    read tag
    echo "Enter release title:"
    read title
    gh release create "$tag" aaron_os.iso --title "$title" --draft --notes "Stable Monolithic Build - April 2026"
    echo "Draft release created on GitHub."
fi

# 4. Boot
if [ ! -f hd.img ]; then qemu-img create -f raw hd.img 10M; fi
echo "Booting AaronOS..."
qemu-system-x86_64 -cdrom aaron_os.iso -drive file=hd.img,format=raw -boot order=cd -m 256M -machine pc -audiodev pa,id=speaker -machine pcspk-audiodev=speaker