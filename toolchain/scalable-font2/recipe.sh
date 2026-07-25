REPO="https://gitlab.com/bztsrc/scalable-font2.git"
REVISION="8ac41c8aaed8c9c992527548d2b76074609da7f6"

function build()
{
    export USE_DYNDEPS=1

    pushd_silent ${SRC_DIR}/sfnconv
    make -j${NPROC} || exit 1
    popd_silent

    pushd_silent ${SRC_DIR}/sfnedit
    make -j${NPROC} || exit 1
    popd_silent
}

function install()
{
    cp ${SRC_DIR}/ssfn.h ${SYSROOT}/usr/include || exit 1
    cp ${SRC_DIR}/sfnconv/sfnconv ${NATIVE_SYSROOT}/bin || exit 1
    cp ${SRC_DIR}/sfnedit/sfnedit ${NATIVE_SYSROOT}/bin || exit 1
}
