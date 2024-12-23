#!/bin/bash

base_dir=$(dirname $0)

. "${base_dir}/utils.sh"

disk_image="disk.img"
disk_size=300
mountpoint=mnt
boot_dir="${mountpoint}/boot"
grub_cfg="${boot_dir}/grub/grub.cfg"
dev=""
create_partition=
mount_loopback=
rebuild=
font=/usr/share/kbd/consolefonts/Lat2-Terminus16.psfu.gz
grub=$(command -v grub-install 2>/dev/null) || die "No grub-install"

while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        --rebuild)
            mount_loopback=1
            rebuild=1 ;;
        -v|--verbose)
            verbose=1 ;;
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
    execute_cmd "creating loopback... " dev=$(sudo losetup --find --partscan --show "${disk_image}")
fi

echo "loopback device: ${dev}"

partition_number="p1"

if [[ ! -z "${create_partition}" ]]
then
    execute_cmd "creating ext2 partition... "   sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 1MiB 100%
    execute_cmd "destroying old filesystem... " sudo dd if=/dev/zero of="${dev}${partition_number}" bs=1M count=1 status=none
    execute_cmd "creating new filesystem... "   sudo mke2fs -q -I 128 "${dev}${partition_number}"
fi

if [[ ! -z "${mount_loopback}" ]]
then
    mkdir -p mnt
    execute_cmd "mounting filesystem... "       sudo mount -t ext2 -o rw "${dev}${partition_number}" ${mountpoint}

    sudo chown -R ${USER} ${mountpoint}
    sudo chgrp -R ${USER} ${mountpoint}
fi

if [[ ! -f "font.psf" ]] || [[ "${0}" -nt "font.psf" ]] || [[ -n ${rebuild} ]]
then
    rm -f font.psf
    cp ${font} font.psf.gz
    gzip -d font.psf.gz
fi

bashrc_content='export PS1="\u \e[34m\w\e[0m # "

function path()
{
    stat -L "${1}" >/dev/null && readlink -f "${1}"
}

alias ktest=/bin/test
alias ..="cd .."
alias l="ls -lah"
alias dmesg="cat /proc/syslog"
alias f="find . -name"
alias p="path"
function k()
{
    while ktest -q -t realloc_small_to_small; do echo -n; done
}
'

create_dir "${mountpoint}/bin"
create_dir "${mountpoint}/dev"
create_dir "${mountpoint}/usr/lib"
create_dir "${mountpoint}/usr/share"
create_dir "${mountpoint}/lib/modules"
create_dir "${mountpoint}/tmp"
create_dir "${mountpoint}/proc"
create_dir "${mountpoint}/root"
create_dir "${mountpoint}/sys"
create_dir "${boot_dir}/grub"

for binary in $(find bin -type f -executable)
do
    if [[ ${binary} == *".so"* ]]
    then
        continue
    fi
    copy ${binary} ${mountpoint}/bin
done

for binary in $(find sysroot -type f -executable)
do
    if [[ ${binary} == *"ld.so" ]]
    then
        copy ${binary} ${mountpoint}/lib
    elif [[ ${binary} == *".so"* ]]
    then
        copy ${binary} ${mountpoint}/usr/lib
    else
        copy ${binary} ${mountpoint}/bin
    fi
done

copy_dir_content "modules" "${mountpoint}/lib/modules"

copy ../arch/x86/cpuid.c ${mountpoint}
copy ../tux.tga ${mountpoint}
copy ../cursor.tga ${mountpoint}
copy ../close.tga ${mountpoint}
copy ../close_pressed.tga ${mountpoint}
copy font.psf ${mountpoint}/usr/share

write_to "${bashrc_content}" "${mountpoint}/root/.bashrc"
copy kernel.map "${boot_dir}"
copy system "${boot_dir}"

if [[ ! -d ${boot_dir}/grub/i386-pc ]]
then
    sudo ${grub} --boot-directory=${boot_dir} --target=i386-pc --modules="ext2 part_msdos" "${dev}"
fi

sync -f ${mountpoint}

cleanup
