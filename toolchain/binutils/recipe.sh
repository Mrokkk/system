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
            --with-system-zlib || die "binutils configuration failed"
    fi
    make -O all -j${NPROC} || die "binutils compilation failed"
}

function install()
{
    make -O install -j${NPROC} || die "binutils installation failed"
}
