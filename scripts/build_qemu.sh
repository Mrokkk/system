#!/bin/bash

set -e

base_dir=$(dirname `readlink -f "$0"`)

toolchain_dir="${base_dir}/../toolchain"
qemu_dir="${toolchain_dir}/qemu"
qemu_build_dir="${PWD}/qemu"
qemu_target="qemu-system-i386"

mkdir -p "${toolchain_dir}"

if [[ ! -d "${qemu_dir}" ]]
then
    git clone --depth 1 https://git.qemu-project.org/qemu.git "${qemu_dir}"
    pushd ${qemu_dir}
    patch -p1 < "${base_dir}/qemu_bm_fix.patch"
    popd
fi

if [[ ! -d "${qemu_build_dir}" ]]
then
    mkdir "${qemu_build_dir}"
    pushd "${qemu_build_dir}"
    "${qemu_dir}/configure" --enable-debug
    popd
fi

pushd "${qemu_build_dir}"
ninja "${qemu_target}"
popd
