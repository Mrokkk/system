#!/bin/bash

base_dir="$(dirname `readlink -f ${0}`)"

. "${base_dir}/utils.sh"

bochs="$(binary_from_native_sysroot bochs)"
bios="$(resource_from_native_sysroot BIOS-bochs-latest)"
vgabios="$(resource_from_native_sysroot VGABIOS-lgpl-latest)"

kernel_params="earlycon syslog=/dev/debug0"

grub_cfg_content="set timeout=0
set default=0
menuentry \"system\" {
    if ! multiboot /boot/system ${kernel_params}; then sleep 4; reboot; fi
    module /boot/kernel.map kernel.map
    boot
}"

grub_cfg="mnt/boot/grub/grub.cfg"
write_to "${grub_cfg_content}" "${grub_cfg}"
sync "${grub_cfg}"

bochsrc_content="""
memory: guest=128, host=128
romimage: file=${bios}, options=fastboot
vgaromimage: file=${vgabios}
vga: extension=vbe
ata0-master: type=disk, path="disk.img", mode=flat, cylinders=16, heads=16, spt=16, translation=lba
boot: disk
mouse: enabled=0
port_e9_hack: enabled=1
log: bochs.log
display_library: sdl2"""

write_to "${bochsrc_content}" ".bochsrc"

${bochs} -f .bochsrc -q

cleanup
