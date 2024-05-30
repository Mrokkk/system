REPO="git://gcc.gnu.org/git/gcc.git"
VERSION="13.2.0"
BRANCH="releases/gcc-${gcc_version}"

function build()
{
    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix="${PREFIX}" \
            --target="${TARGET}" \
            --with-sysroot="${SYSROOT}" \
            --disable-nls \
            --enable-languages=c \
            --disable-gcov || die "configuration failed"
    fi

    make all-gcc -j${NPROC}           || die "gcc compilation failed"
    make all-target-libgcc -j${NPROC} || die "libgcc compilation failed"
}

function install()
{
    make install-gcc -j${NPROC}           || die "gcc installation failed"
    make install-target-libgcc -j${NPROC} || die "libgcc installation failed"
}
