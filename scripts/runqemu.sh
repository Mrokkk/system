#!/usr/bin/env bash

use_kvm=
use_nographic=
args=""

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
        *)
            args="${args} ${1}"
            ;;
    esac
    shift
done

[[ -f disk.img ]] && args="${args} -drive file=disk.img,format=raw,id=disk0,media=disk -device ich9-ahci"
#[[ -f disk.img ]] && args="${args} -drive file=disk.img,format=raw,id=disk0,media=disk,if=none -device ahci,id=ahci -device ide-hd,drive=disk0,bus=ahci.0"

if [[ -z ${use_nographic} ]]
then
    args="${args} -display gtk,zoom-to-fit=off"
else
    args="${args} -display none"
fi

if [[ -z ${use_kvm} ]]
then
    args="${args} -cpu qemu64"
else
    args="${args} -enable-kvm -cpu host"
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

echo "Args: ${args}"

exec $(which qemu-system-i386 || which qemu-system-x86_64) \
    ${args} \
    -cdrom system.iso \
    -boot once=d \
    -no-reboot \
    -rtc base=localtime,clock=host \
    -device isa-debugcon,chardev=char0 \
    -chardev stdio,id=char0,mux=on,signal=off \
    -chardev pipe,id=char1,path=ttyS0 \
    -mon chardev=char0 \
    -serial chardev:char1 \
    -usb
