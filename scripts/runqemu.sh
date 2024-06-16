#!/usr/bin/env bash

set -e

base_dir="$(dirname `readlink -f ${0}`)"

. "${base_dir}/utils.sh"

use_ide=
use_kvm=
use_cdrom=
use_nographic=
args="\
-boot once=c \
-no-reboot \
-rtc base=localtime,clock=host \
-device isa-debugcon,chardev=char0 \
-chardev stdio,id=char0,mux=on,signal=off \
-chardev pipe,id=char1,path=ttyS0 \
-mon chardev=char0 \
-serial chardev:char1 \
-usb"

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
        *)
            args="${args} ${1}"
            ;;
    esac
    shift
done

qemu_path=$(binary_from_native_sysroot "qemu-system-i386")

if [[ -z "${qemu_path}" ]]
then
    qemu_path="$(which qemu-system-i386 || which qemu-system-x86_64)"
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
    args="${args} -display gtk,zoom-to-fit=off"
else
    args="${args} -display none"
fi

if [[ -z ${use_kvm} ]]
then
    args="-cpu qemu64 ${args}"
else
    args="-enable-kvm -cpu host ${args}"
    if [[ -z ${use_nographic} ]]
    then
        args="${args} -device virtio-gpu,edid=on,xres=1600,yres=900"
    fi
fi

if [[ -f "dmi.bin" ]]
then
    args="${args} -smbios file=dmi.bin"
fi

if [[ ! -p ttyS0.in ]]
then
    rm -rf ttyS0.in ttyS0.out
    mkfifo ttyS0.in
    mkfifo ttyS0.out
fi

echo "Command: ${qemu_path} ${args}"

exec ${base_dir}/runqemu_wrapper.py "${qemu_path}" ${args}
