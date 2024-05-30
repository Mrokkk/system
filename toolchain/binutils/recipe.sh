REPO="git://sourceware.org/git/binutils-gdb.git"
VERSION="2.42"
BRANCH=binutils-$(echo "${binutils_version}" | tr "." "_")-branch

function build()
{
    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix="${PREFIX}" \
            --target="${TARGET}" \
            --with-sysroot="${SYSROOT}" \
            --enable-shared \
            --disable-nls \
            --disable-gdb || die "binutils configuration failed"
    fi
    make all || die "binutils compilation failed"
}

function install()
{
    make install -j${NPROC} || die "binutils installation failed"
}
