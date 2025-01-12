REPO="https://gitlab.com/bztsrc/scalable-font2.git"

function build()
{
    export USE_DYNDEPS=1

    pushd_silent ${SRC_DIR}/sfnconv
    make -j${NPROC}
    popd_silent

    pushd_silent ${SRC_DIR}/sfnedit
    make -j${NPROC}
    popd_silent
}

function install()
{
    cp ${SRC_DIR}/ssfn.h ${SYSROOT}/usr/include
    cp ${SRC_DIR}/sfnconv/sfnconv ${NATIVE_SYSROOT}/bin
    cp ${SRC_DIR}/sfnedit/sfnedit ${NATIVE_SYSROOT}/bin
}
