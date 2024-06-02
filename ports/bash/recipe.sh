REPO="https://github.com/bminor/bash.git"
VERSION=
BRANCH=

function build()
{
    export CC="${NATIVE_SYSROOT}/bin/i686-pc-phoenix-gcc"

    [[ ! -f "${CC}" ]] && die "Toolchain not built"

    [[ -f "Makefile" ]] && make distclean

    ${SRC_DIR}/configure \
        --prefix=${SYSROOT} \
        --host i686-pc-phoenix \
        --enable-minimal-config \
        --disable-largefile \
        --disable-nls \
        --disable-threads \
        --disable-multibyte || die "configuration failed"

    make bash -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O install -j${NPROC}
}
