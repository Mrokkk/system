#!/bin/bash

set -e

base_dir=$(dirname `readlink -f "$0"`)

toolchain_dir="${base_dir}/../toolchain"
bochs_dir="${toolchain_dir}/bochs"
bochs_build_dir="${PWD}/bochs"

mkdir -p "${toolchain_dir}"

if [[ ! -d "${bochs_dir}" ]]
then
    svn checkout https://svn.code.sf.net/p/bochs/code/trunk/bochs ${bochs_dir}
fi

if [[ ! -d "${bochs_build_dir}" ]]
then
    mkdir "${bochs_build_dir}"
    pushd "${bochs_build_dir}"
    ${bochs_dir}/configure --enable-smp \
        --enable-cpu-level=6 \
        --enable-all-optimizations \
        --enable-x86-64 \
        --enable-pci \
        --enable-vmx \
        --enable-voodoo \
        --enable-debugger \
        --enable-debugger-gui \
        --enable-logging \
        --enable-fpu \
        --enable-3dnow \
        --enable-cdrom \
        --enable-x86-debugger \
        --enable-iodebug \
        --disable-plugins \
        --disable-docbook \
        --enable-idle-hack \
        --with-x --with-x11 --with-term --with-sdl2
    popd
fi

pushd "${bochs_build_dir}"
make -j$(nproc)
popd
