#!/usr/bin/env bash

base_dir=$(dirname `readlink -f "$0"`)

bochs_dir="${base_dir}/../toolchain/bochs"
bochs="${PWD}/bochs/bochs"
bios="${bochs_dir}/bios/BIOS-bochs-latest"
vgabios="${bochs_dir}/bios/VGABIOS-lgpl-latest"

echo """
memory: guest=128, host=128
romimage: file=${bios}, options=fastboot
vgaromimage: file=${vgabios}
vga: extension=vbe
ata0-master: type=disk, path="disk.img", mode=flat, cylinders=16, heads=16, spt=16, translation=lba
ata0-slave: type=cdrom, path="system.iso", status=inserted
boot: cdrom
mouse: enabled=0
port_e9_hack: enabled=1
log: bochs.log
display_library: sdl2
""" > .bochsrc

${bochs} -f .bochsrc -q
exit 0
