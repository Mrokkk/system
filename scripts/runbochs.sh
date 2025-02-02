#!/bin/bash

base_dir="$(dirname `readlink -f ${0}`)"

. "${base_dir}/utils.sh"

gpu="banshee"

while [[ $# -gt 0 ]]; do
    case "${1}" in
        --voodoo1)
            gpu="voodoo1"
            ;;
        --voodoo2)
            gpu="voodoo2"
            ;;
        --voodoo3)
            gpu="voodoo3"
            ;;
        --banshee)
            gpu="banshee"
            ;;
        --vga)
            gpu="vga"
            ;;
        *)
            die "Unsupported param: \"${1}\""
    esac
    shift
done

bochs="$(binary_from_native_sysroot bochs)"
bios="$(resource_from_native_sysroot BIOS-bochs-latest)"

if [[ "${gpu}" == *"voodoo"* ]]
then
    vgabios="$(resource_from_native_sysroot VGABIOS-lgpl-latest-banshee)"
    gpu_entry="vga: extension=voodoo
voodoo: model=${gpu}"
elif [[ "${gpu}" == *"banshee"* ]]
then
    vgabios="$(resource_from_native_sysroot VGABIOS-lgpl-latest-banshee)"
    gpu_entry="vga: extension=voodoo
voodoo: model=banshee
pci: chipset=i440bx, slot5=voodoo"
else
    vgabios="$(resource_from_native_sysroot VGABIOS-lgpl-latest)"
    gpu_entry="vga: extension=vbe"
fi

kernel_params="earlycon syslog=/dev/debug0 pciprint"

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
${gpu_entry}
ata0-master: type=disk, path="disk.img", mode=flat, cylinders=16, heads=16, spt=16, translation=lba
boot: disk
mouse: enabled=0
port_e9_hack: enabled=1
log: bochs.log
display_library: sdl2"""

write_to "${bochsrc_content}" ".bochsrc"

${bochs} -f .bochsrc -q

cleanup
