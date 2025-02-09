REPO="https://git.savannah.gnu.org/git/grub.git"
BRANCH="grub-2.12-rc1"

function build()
{
    if [[ ! -f Makefile ]]
    then
        pushd_silent ${SRC_DIR}
        ./bootstrap || exit 1
        popd_silent

        ${SRC_DIR}/configure \
            --prefix="${PREFIX}" \
            --program-prefix="" \
            --disable-nls \
            --disable-silent-rules \
            --with-platform="pc" \
            --target="i386" \
            --enable-boot-time \
            --disable-werror || exit 1
    fi

    make -O -j${NPROC} || exit 1
}

function install()
{
    make -O install -j${NPROC} || exit 1
}
