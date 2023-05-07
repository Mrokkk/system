#!/bin/bash

set -e

disk_image="disk.img"
disk_size=2
mountpoint=mnt
dev=""
create_partition=
mount_loopback=
rebuild=
stderr=$(mktemp)
font=/usr/share/kbd/consolefonts/Lat2-Terminus16.psfu.gz

function die()
{
    echo "${@}"
    if [[ -f ${stderr} ]]
    then
        echo "Error:"
        cat ${stderr}
        rm -f ${stderr}
    fi
    exit -1
}

function execute_cmd()
{
    local cmd="${@:2}"
    echo -n "${1}"
    eval ${cmd} &>${stderr} || die "failed"
    echo "done"
}

function copy()
{
    local src=${1}
    local dest=${2}/$(basename ${src})
    if [[ ${src} -nt ${dest} ]]
    then
        execute_cmd "copying ${src} to ${dest}... " rsync ${src} ${dest}
    else
        echo "${dest}: up to date"
    fi
}

while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        --rebuild)
            mount_loopback=1
            rebuild=1 ;;
        *)
            break ;;
    esac
    shift
done

if mountpoint -q ${mountpoint}
then
    dev=$(mount | grep "${PWD}/${mountpoint}" | awk '{print $1;}' | sed -e 's/p[0-9]$//')
    if [[ ! -z "${rebuild}" ]]
    then
        execute_cmd "unmounting previous file system... " sudo umount "${mountpoint}"
        rm -rf "${mountpoint}"
        if [[ ! -z "${dev}" ]]
        then
            execute_cmd "removing loopback ${dev}... " sudo losetup -d "${dev}"
            dev=
        fi
    fi
else
    mount_loopback=1
fi

if [[ ! -f ${disk_image} ]] || [[ ! -z "${rebuild}" ]]
then
    create_partition=1

    rm -rf "${disk_image}"
    execute_cmd "creating new image... " dd if=/dev/zero of="${disk_image}" bs=1M count="${disk_size}" status=none
fi

if [[ -z "${dev}" ]]
then
    echo -n "creating loopback... "
    dev=$(sudo losetup --find --partscan --show "${disk_image}" 2>${stderr})
fi

[[ -z "${dev}" ]] && die "failed"

echo "done; dev = ${dev}"

partition_number="p1"

if [[ ! -z "${create_partition}" ]]
then
    execute_cmd "creating ext2 partition... "   sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 512B 100%
    execute_cmd "destroying old filesystem... " sudo dd if=/dev/zero of="${dev}${partition_number}" bs=1M count=1 status=none
    execute_cmd "creating new filesystem... "   sudo mke2fs -q -I 128 "${dev}${partition_number}"
fi

if [[ ! -z "${mount_loopback}" ]]
then
    mkdir -p mnt
    execute_cmd "mounting filesystem... "       sudo mount -t ext2 -o rw "${dev}${partition_number}" ${mountpoint}

    sudo chmod -R a+w ${mountpoint}
    sudo chown -R ${USER} ${mountpoint}
    sudo chgrp -R ${USER} ${mountpoint}
fi

mkdir -p ${mountpoint}/bin
mkdir -p ${mountpoint}/dev
mkdir -p ${mountpoint}/usr/share
mkdir -p ${mountpoint}/lib
mkdir -p ${mountpoint}/tmp
mkdir -p ${mountpoint}/proc
for binary in $(find bin -type f -executable)
do
    copy ${binary} ${mountpoint}/bin
done
copy ../arch/x86/cpuid.c ${mountpoint}
copy ../tux.tga ${mountpoint}

if [ ! -f "font.psf" ] || [ "${0}" -nt "font.psf" ] || [ ! -z ${rebuild} ]
then
    rm -f font.psf
    cp ${font} font.psf.gz
    gzip -d font.psf.gz
    copy font.psf ${mountpoint}/usr/share
fi

sync

[[ -f ${stderr} ]] && rm -f "${stderr}"
