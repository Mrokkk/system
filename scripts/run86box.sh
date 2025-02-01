#!/bin/bash

base_dir="$(dirname `readlink -f ${0}`)"

. "${base_dir}/utils.sh"

box86="$(binary_from_native_sysroot 86Box)"

kernel_params="earlycon syslog=/dev/debug0 noapm pciprint vesaprint"

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

if [[ ! -f "86box.cfg" ]]
then
    cp ../scripts/86box.cfg .
fi

${base_dir}/emulator_wrapper.py "${box86}" --logfile 86box.log

cleanup
