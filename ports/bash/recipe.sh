REPO="https://github.com/bminor/bash.git"
VERSION=
BRANCH=

function build()
{
    export CC="${NATIVE_SYSROOT}/bin/i686-pc-phoenix-gcc"
    export CFLAGS="-ggdb3"
    export PATH="${NATIVE_SYSROOT}/bin:${PATH}"

    [[ ! -f "${CC}" ]] && die "Toolchain not built"

    #[[ -f "Makefile" ]] && make distclean

    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix=${SYSROOT} \
            --host=i686-pc-phoenix \
            --target=i686-pc-phoenix \
            --enable-minimal-config \
            --enable-alias \
            --disable-largefile \
            --disable-nls \
            --disable-threads \
            --without-bash-malloc \
            --disable-multibyte || die "configuration failed"
    fi

    make bash -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O install -j${NPROC}
}
