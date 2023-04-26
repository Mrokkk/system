#!/bin/bash

set -e

root_dir=root
disk_image="disk.img"
disk_size=2

function die() {
    echo "${@}"
    exit -1
}

rm -rf ${disk_image}

dd if=/dev/zero of="${disk_image}" bs=1M count="${disk_size}" status=none
dev=$(sudo losetup --find --partscan --show "${disk_image}")

[[ -z "${dev}" ]] && die "cannot create loopback"

echo "${dev}"

echo "creating ext2 partition..."
sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 1MiB 100% || die "cannot partition image"
echo "done"
partition_number="p1"
partition_scheme="mbr"

echo "destroying old filesystem... "
sudo dd if=/dev/zero of="${dev}${partition_number}" bs=1M count=1 status=none || die "cannot destroy old filesystem"
echo "done"

echo "creating new filesystem... "
sudo mke2fs -q -I 128 "${dev}${partition_number}" || die "cannot create ext2"
echo "done"

echo "mounting filesystem..."
mkdir -p mnt
sudo mount -t ext2 -o rw "${dev}${partition_number}" mnt/ || die "cannot mount"
echo "done"

sudo chmod -R a+w mnt
sudo chown -R ${USER} mnt
sudo chgrp -R ${USER} mnt

echo "this is test" > mnt/test_file
cp ../fs/file_system.c mnt
sync
