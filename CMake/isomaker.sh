#!/usr/bin/env bash

mkdir -p iso/boot/grub

if [ ! -f iso/boot/grub/menu.lst ]; then
    echo default 0 > iso/boot/grub/menu.lst
    echo timeout 0 >> iso/boot/grub/menu.lst
    echo title My kernel >> iso/boot/grub/menu.lst
    echo kernel /kernel >> iso/boot/grub/menu.lst
    # echo module /symbols >>  iso/boot/grub/menu.lst
    echo boot >> iso/boot/grub/menu.lst
fi

if [ ! -f iso/boot/grub/stage2_eltorito ]; then
    echo Downloading bootloader stage2...
    wget -q ftp://ftp.free.org/mirrors/rsync.frugalware.org/frugalware-testing/boot/grub/stage2_eltorito
    mv stage2_eltorito iso/boot/grub/stage2_eltorito
fi

cp system iso/kernel
# cp bin/kernel.sym iso/symbols
mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o os.iso iso 2> /dev/null
