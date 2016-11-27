#!/usr/bin/env bash

base_dir=$(dirname $0)

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
    cp $base_dir/stage2_eltorito iso/boot/grub/stage2_eltorito
fi

cp system iso/kernel
# cp bin/kernel.sym iso/symbols
MKISO=$(which genisoimage || which mkisofs)
$MKISO -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o os.iso iso 2> /dev/null
