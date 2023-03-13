#!/usr/bin/env bash

args=""

while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        -g|--gdb)
            args+="--gdb tcp::9000 -S"
            shift ;;
        -n|--no-graphic)
            args+="-display none"
            shift ;;
        -k|--kvm)
            args+="-enable-kvm"
            shift ;;
        *)
            break ;;
    esac
    shift
done

exec $(which qemu-system-i386 || which qemu-system-x86_64) \
    -cdrom system.iso \
    -no-reboot \
    -vga std \
    -chardev stdio,id=char0,signal=off,mux=on \
    -chardev file,id=char1,path=logs \
    -serial chardev:char0 \
    -serial chardev:char1 \
    -mon chardev=char0 ${args} | tee kernel.log
