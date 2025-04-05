REPO="git://gcc.gnu.org/git/gcc.git"
VERSION="13.2.0"
BRANCH="releases/gcc-${VERSION}"

function build()
{
    export CFLAGS="-g -O2"
    export CXXFLAGS="-g -O2"

    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix="${PREFIX}" \
            --target="${TARGET}" \
            --with-sysroot="${SYSROOT}" \
            --disable-nls \
            --enable-languages=c \
            --disable-gcov \
            --disable-bootstrap \
            --enable-checking=release \
            --enable-shared \
            --enable-default-pie \
            --enable-host-shared \
            --with-system-zlib || exit 1
    fi

    make -O all-gcc -j${NPROC} || exit 1
    make -O all-target-libgcc -j${NPROC} || exit 1
}

function install()
{
    make -O install-gcc -j${NPROC} || exit 1
    make -O install-target-libgcc -j${NPROC} || exit 1
}
