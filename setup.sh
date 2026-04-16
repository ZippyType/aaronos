#!/bin/bash

# --- 1. COMMIT STEP ---
echo "Checking for changes..."
if [ -n "$(git status --porcelain)" ]; then
    echo "Files modified. Enter commit message (leave blank to skip):"
    read commit_msg
    if [ -n "$commit_msg" ]; then
        git add .
        git commit -m "$commit_msg"
        git push -u origin main --force
    fi
fi

# --- 2. COMPILE & ERROR CHECKING ---
echo "Cleaning up..."
rm -f *.o kernel.elf aaron_os.iso
ERRORS=""

# Compile function remains
compile_file() {
    echo "Compiling $1..."
    gcc -m32 -c $1 -o $2 -ffreestanding -O2 -fno-stack-protector
    if [ $? -ne 0 ]; then ERRORS+="$1 "; fi
}

nasm -f elf32 boot.s -o boot.o
if [ $? -ne 0 ]; then ERRORS+="boot.s "; fi

compile_file "keyboard.c" "keyboard.o"
compile_file "installer.c" "installer.o"
compile_file "editor.c" "editor.o"
compile_file "fat16.c" "fat16.o"
compile_file "memory.c" "memory.o"
compile_file "gui.c" "gui.o"
compile_file "kernel.c" "kernel.o"

if [ -n "$ERRORS" ]; then
    echo "ABORT: Errors detected in: $ERRORS"; exit 1
fi

ld -m elf_i386 -T linker.ld -o kernel.elf boot.o keyboard.o installer.o editor.o fat16.o memory.o gui.o kernel.o --no-warn-rwx-segments
mkdir -p iso_root/boot/grub
cp kernel.elf iso_root/boot/
grub-mkrescue -o aaron_os.iso iso_root

# --- 3. DYNAMIC RELEASE MANAGER ---
echo "Build successful! Create a GitHub draft release? [y/N]"
read release_choice
if [[ "$release_choice" == [Yy]* ]]; then
    
    # List Tags
    echo "Select a Tag (or type new):"
    tags=($(git tag -l))
    for i in "${!tags[@]}"; do printf "%d) %s\n" "$((i+1))" "${tags[$i]}"; done
    read tag_choice
    tag="${tags[$((tag_choice-1))]}"
    [ -z "$tag" ] && read -p "Type custom tag: " tag
    
    # Choose Notes Method
    echo "Release Notes: 1) Select .MD file from home, 2) Write custom notes"
    read note_choice
    
    notes=""
    if [ "$note_choice" == "1" ]; then
        md_files=($(find $HOME -name "*.md"))
        for i in "${!md_files[@]}"; do printf "%d) %s\n" "$((i+1))" "${md_files[$i]}"; done
        read md_choice
        notes=$(cat "${md_files[$((md_choice-1))]}")
    else
        echo "Enter your Markdown release notes (Press Ctrl+D when finished):"
        notes=$(cat)
    fi
    
    gh release create "$tag" aaron_os.iso --title "Release $tag" --draft --notes "$notes"
    echo "Draft release $tag created."
fi

# --- 4. BOOT ---
if [ ! -f hd.img ]; then qemu-img create -f raw hd.img 10M; fi
qemu-system-x86_64 -cdrom aaron_os.iso -drive file=hd.img,format=raw -boot order=cd -m 256M -machine pc -audiodev pa,id=speaker -machine pcspk-audiodev=speaker