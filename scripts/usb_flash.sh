#!/bin/bash

set -e

device="${1}"

if [[ -z "${device}" ]]
then
    echo "Device must be passed!"
fi

device="/dev/${device}"

make iso -j$(nproc)

while ! ls ${device} &>/dev/null
do
    sleep 0.2
done

sudo dd if=system.iso of="${device}" bs=8M status=progress oflag=direct
sudo eject "${device}"
