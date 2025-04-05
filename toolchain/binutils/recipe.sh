REPO="git://sourceware.org/git/binutils-gdb.git"
VERSION="2.42"
BRANCH=binutils-$(echo "${VERSION}" | tr "." "_")-branch

function build()
{
    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix="${PREFIX}" \
            --target="${TARGET}" \
            --with-sysroot="${SYSROOT}" \
            --enable-shared \
            --enable-default-pie \
            --disable-nls \
            --disable-gdb \
            --disable-gdbserver \
            --disable-libdecnumber \
            --disable-threads \
            --disable-sim \
            --with-system-zlib || exit 1
    fi
    make -O all -j${NPROC} || exit 1
}

function install()
{
    make -O install -j${NPROC} || exit 1
}
