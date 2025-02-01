#!/bin/bash

base_dir="$(dirname `readlink -f ${0}`)"

. "${base_dir}/utils.sh"

use_ide=
use_kvm=
use_cdrom=
use_nographic=
use_isa_debugcon=
xres=1920
yres=1124
default_gpu="-device virtio-gpu,edid=on,xres=${xres},yres=${yres} -vga none"
gpu="${default_gpu}"
args="\
-boot once=c \
-no-reboot \
-no-shutdown \
-rtc base=utc,clock=host \
-chardev stdio,id=char0,mux=on,signal=off \
-chardev pipe,id=char1,path=ttyS0 \
-chardev pipe,id=char2,path=qemumon \
-mon chardev=char2 \
-serial chardev:char1 \
-usb"
kernel_params=

supported_kernel_params_value=(
    "console"
    "clock"
    "init"
    "syslog"
    "systick"
)

supported_kernel_params_bool=(
    "earlycon"
    "pciprint"
    "vesaprint"
    "nomodeset"
    "noapm"
)

declare -A kernel_params_dict
kernel_params_dict["syslog"]="/dev/vcon0"
kernel_params_dict["console"]="/dev/tty0"

while [[ $# -gt 0 ]]; do
    case "${1}" in
        -g|--gdb)
            args="${args} --gdb tcp::9000 -S"
            ;;
        -n|--no-graphic)
            use_nographic=1
            ;;
        -k|--kvm)
            use_kvm=1
            ;;
        -p|--qemu-path)
            qemu_path="${2}"
            shift
            ;;
        --ide)
            use_ide=1
            ;;
        --ahci)
            use_ide=
            ;;
        --cdrom)
            use_cdrom=1
            ;;
        --isa-debugcon)
            use_isa_debugcon=1
            ;;
        --virtio-vga)
            gpu="-device virtio-vga"
            ;;
        --isa-vga)
            gpu="-device isa-vga"
            ;;
        --pci-vga)
            gpu="-device VGA"
            ;;
        --bochs-vga)
            gpu="-device bochs-display"
            ;;
        --cirrus-vga)
            gpu="-device cirrus-vga"
            ;;
        --ati-vga)
            gpu="-device ati-vga"
            ;;
        --vmware-svga)
            gpu="-device vmware-svga"
            ;;
        --qxl-vga)
            gpu="-device qxl-vga"
            ;;
        --virtio-gpu)
            gpu="${default_gpu}"
            ;;
        *)
            param="${1#--}"
            if [[ ${supported_kernel_params_value[@]} =~ ${param} ]]
            then
                kernel_params_dict["${param}"]=${2}
                shift
            elif [[ ${supported_kernel_params_bool[@]} =~ ${param} ]]
            then
                kernel_params_dict["${param}"]=true
            else
                args="${args} ${1}"
            fi
            ;;
    esac
    shift
done

if file system | grep -q 32; then
    qemu_path=$(binary_from_native_sysroot "qemu-system-i386")
else
    qemu_path=$(binary_from_native_sysroot "qemu-system-x86_64")
fi

if [[ -z "${qemu_path}" ]]
then
    qemu_path="$(which qemu-system-x86_64 || which qemu-system-i386)"
fi

if [[ -n "${use_isa_debugcon}" ]]
then
    args="${args} -device isa-debugcon,chardev=char0"
    kernel_params_dict["syslog"]="/dev/debug0"
else
    args="${args} -device virtio-serial -device virtconsole,chardev=char0,name=serial0"
fi

if [[ -z "${use_ide}" ]]
then
    args="${args} -device ahci,id=ahci"
    [[ -f disk.img ]] && args="${args} -drive file=disk.img,format=raw,id=disk0,media=disk,if=none -device ide-hd,drive=disk0,bus=ahci.0"
    [[ -n "${use_cdrom}" ]] && [[ -f "system.iso" ]] && args="${args} -drive file=system.iso,format=raw,id=cdrom0,media=cdrom,if=none -device ide-cd,drive=cdrom0,bus=ahci.1"
else
    [[ -f disk.img ]] && args="${args} -drive file=disk.img,format=raw,id=disk0,media=disk"
    [[ -n "${use_cdrom}" ]] && [[ -f "system.iso" ]] && args="${args} -cdrom system.iso"
fi

if [[ -z ${use_nographic} ]]
then
    args="${args} -display gtk,zoom-to-fit=off ${gpu}"
else
    args="${args} -display none"
fi

if [[ -z ${use_kvm} ]]
then
    args="-cpu qemu64 ${args}"
else
    args="-enable-kvm -cpu host ${args}"
fi

if [[ -f "dmi.bin" ]]
then
    args="${args} -smbios file=dmi.bin"
fi

if [[ ! -p ttyS0.in ]]
then
    rm -rf ttyS0.in ttyS0.out qemumon.in qemumon.out
    mkfifo ttyS0.in
    mkfifo ttyS0.out
    mkfifo qemumon.in
    mkfifo qemumon.out
fi

for x in "${!kernel_params_dict[@]}"
do
    if [[ "${kernel_params_dict[$x]}" == "true" ]]
    then
        kernel_params="${kernel_params} ${x}"
    else
        kernel_params="${kernel_params} ${x}=${kernel_params_dict[$x]}"
    fi
done

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

echo "Command: ${qemu_path} ${args}"
echo "Kernel params:${kernel_params}"

${base_dir}/emulator_wrapper.py "${qemu_path}" ${args}

cleanup
