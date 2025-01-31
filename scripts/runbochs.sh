#!/bin/bash

base_dir="$(dirname `readlink -f ${0}`)"

. "${base_dir}/utils.sh"

bochs="$(binary_from_native_sysroot bochs)"
bios="$(resource_from_native_sysroot BIOS-bochs-latest)"
vgabios="$(resource_from_native_sysroot VGABIOS-lgpl-latest)"

echo """
memory: guest=128, host=128
romimage: file=${bios}, options=fastboot
vgaromimage: file=${vgabios}
vga: extension=vbe
ata0-master: type=disk, path="disk.img", mode=flat, cylinders=16, heads=16, spt=16, translation=lba
boot: disk
mouse: enabled=0
port_e9_hack: enabled=1
log: bochs.log
display_library: sdl2
""" > .bochsrc

${bochs} -f .bochsrc -q
exit 0
