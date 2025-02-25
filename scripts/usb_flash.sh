#!/bin/bash

set -e

device="${1}"

if [[ -z "${device}" ]]
then
    echo "Device must be passed!"
    exit 1
fi

device="/dev/${device}"

make iso -j$(nproc)

while ! ls ${device} &>/dev/null
do
    sleep 0.2
done

vendor="$(cat /sys/block/${1}/device/vendor | awk '{$1=$1};1')"
model="$(cat /sys/block/${1}/device/model | awk '{$1=$1};1')"

read -p "Flash device ${vendor} ${model}? y/n " answer

case ${answer:0:1} in
    y|Y )
        :
    ;;
    *)
        exit
    ;;
esac

sudo dd if=system.iso of="${device}" bs=8M status=progress oflag=direct
sudo eject "${device}"
