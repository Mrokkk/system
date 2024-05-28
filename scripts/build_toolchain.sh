#!/bin/bash

base_dir=$(dirname `readlink -f "$0"`)

. "${base_dir}/utils.sh"

target="i686-pc-phoenix"

toolchain_dir="${base_dir}/../toolchain"
build_dir="${PWD}/toolchain"
prefix="${build_dir}/prefix"
sysroot="${build_dir}/sysroot"

binutils_repo="git://sourceware.org/git/binutils-gdb.git"
binutils_src_dir="${toolchain_dir}/binutils"
binutils_build_dir="${build_dir}/binutils"
binutils_version="2.42"

gcc_repo="git://gcc.gnu.org/git/gcc.git"
gcc_src_dir="${toolchain_dir}/gcc"
gcc_build_dir="${build_dir}/gcc"
gcc_version="13.2.0"

newlib_src_dir="${toolchain_dir}/newlib"
newlib_build_dir="${build_dir}/newlib"

nproc=$(nproc)

components=("binutils" "gcc")

function binutils_prepare()
{
    if [[ ! -d "${binutils_src_dir}" ]]
    then
        git clone --branch binutils-$(echo "${binutils_version}" | tr "." "_")-branch --depth 1 "${binutils_repo}" "${binutils_src_dir}"
        pushd "${binutils_src_dir}"
        patch -p1 < "${base_dir}/binutils.patch"
        popd
    fi
}

function binutils_build()
{
    if [[ ! -d "${binutils_build_dir}" ]] || [[ ! -f "${binutils_build_dir}/Makefile" ]]
    then
        create_dir "${binutils_build_dir}"
        pushd "${binutils_build_dir}"
        ${binutils_src_dir}/configure \
            --prefix="${prefix}" \
            --target="${target}" \
            --with-sysroot="${sysroot}" \
            --enable-shared \
            --disable-nls || die "binutils: configuration failed"
    else
        pushd "${binutils_build_dir}"
    fi

    make all -j${nproc} || die "binutils: compilation failed"
    make install -j${nproc} || die "binutils: installation failed"
    popd
}

function gcc_prepare()
{
    if [[ ! -d "${gcc_src_dir}" ]]
    then
        git clone --branch "releases/gcc-${gcc_version}" --depth 1 "${gcc_repo}" "${gcc_src_dir}"
        pushd "${gcc_src_dir}"
        patch -p1 < "${base_dir}/gcc.patch"
        popd
    fi
}

function gcc_build()
{
    if [[ ! -d "${gcc_build_dir}" ]] || [[ ! -f "${gcc_build_dir}/Makefile" ]]
    then
        create_dir "${gcc_build_dir}"
        pushd "${gcc_build_dir}"
        ${gcc_src_dir}/configure \
            --prefix="${prefix}" \
            --target="${target}" \
            --with-sysroot="${sysroot}" \
            --disable-nls \
            --enable-languages=c \
            --disable-gcov || die "Failed to configure GCC"
    else
        pushd "${gcc_build_dir}"
    fi

    make all-gcc -j${nproc} || die "gcc: compilation failed"
    make all-target-libgcc -j${nproc} || die "gcc: libgcc compilation failed"
    make install-gcc -j${nproc} || die "gcc: installation failed"
    make install-target-libgcc -j${nproc} || die "gcc: libgcc installation failed"
    popd
}

echo "Prefix: ${prefix}"
echo "Sysroot: ${sysroot}"

create_dir "${build_dir}"
create_dir "${prefix}"
create_dir "${sysroot}"
create_dir "${sysroot}/usr/include"
copy_dir_content "${toolchain_dir}/../lib/include" "${sysroot}/usr/include"
copy_dir_content "${toolchain_dir}/../include" "${sysroot}/usr/include"

for component in "${components[@]}"
do
    eval "${component}_prepare"
    eval "${component}_build"
done

cleanup
