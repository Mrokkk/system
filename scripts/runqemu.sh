#!/usr/bin/env bash

args=""

while [[ $# -gt 0 ]]; do
    case "${1}" in
        -g|--gdb)
            args="${args} --gdb tcp::9000 -S"
            ;;
        -n|--no-graphic)
            args="${args} -display none"
            ;;
        -k|--kvm)
            args="${args} -enable-kvm"
            ;;
        *)
            args="${args} ${1}"
            ;;
    esac
    shift
done

[[ -f disk.img ]] && args="${args} -drive file=disk.img,format=raw,if=virtio,id=disk0"

echo "Additional args for qemu: ${args}"

exec $(which qemu-system-i386 || which qemu-system-x86_64) \
    -cdrom system.iso \
    -boot once=d \
    -no-reboot \
    -cpu host,rdtscp,invtsc \
    -vga std \
    -chardev stdio,id=char0,signal=off,mux=on \
    -chardev file,id=char1,path=logs \
    -serial chardev:char0 \
    -serial chardev:char1 \
    -mon chardev=char0 ${args}
