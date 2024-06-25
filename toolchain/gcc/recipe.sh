REPO="git://gcc.gnu.org/git/gcc.git"
VERSION="13.2.0"
BRANCH="releases/gcc-${gcc_version}"

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
            --enable-host-shared \
            --with-system-zlib || die "configuration failed"
    fi

    make -O all-gcc -j${NPROC}           || die "gcc compilation failed"
    make -O all-target-libgcc -j${NPROC} || die "libgcc compilation failed"
}

function install()
{
    make -O install-gcc -j${NPROC}           || die "gcc installation failed"
    make -O install-target-libgcc -j${NPROC} || die "libgcc installation failed"
}
