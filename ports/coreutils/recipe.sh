REPO="https://github.com/coreutils/coreutils.git"
VERSION=
BRANCH=

function build()
{
    export CC="${NATIVE_SYSROOT}/bin/i686-pc-phoenix-gcc"
    export CFLAGS="-Wno-error -fdiagnostics-color=always -ggdb3"
    export PATH="${NATIVE_SYSROOT}/bin:${PATH}"

    if [[ ! -f "${SRC_DIR}/configure" ]]
    then
        pushd_silent "${SRC_DIR}"
        ${SRC_DIR}/bootstrap --skip-po
        popd_silent
    fi

    #[[ -f "Makefile" ]] && make distclean

    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix=${SYSROOT} \
            --host=i686-pc-phoenix \
            --target=i686-pc-phoenix \
            --disable-largefile \
            --disable-nls \
            --disable-threads \
            --disable-acl \
            --disable-xattr \
            --disable-libsmack \
            --disable-libcap \
            --disable-werror \
            --disable-multibyte \
            --enable-no-install-program=arch,coreutils,hostname,chcon,runcon,df,pinky,users,who,nice || die "configuration failed"
    fi

    make -O -j${NPROC}
}

function install()
{
    make -O install -j${NPROC}
}
