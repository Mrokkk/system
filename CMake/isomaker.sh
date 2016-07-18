#!/usr/bin/env bash

if [ ! -f $1/iso/boot/grub/stage2_eltorito ]; then
    echo Downloading bootloader stage2...
    wget -q ftp://ftp.free.org/mirrors/rsync.frugalware.org/frugalware-testing/boot/grub/stage2_eltorito
    mv stage2_eltorito $1/iso/boot/grub/stage2_eltorito
fi

mkdir -p $1/iso/boot/grub
cp $1/bin/system $1/iso/kernel
# cp $1/bin/kernel.sym iso/symbols
mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o $1/os.iso $1/iso 2> /dev/null
