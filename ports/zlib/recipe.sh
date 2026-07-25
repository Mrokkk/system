REPO="https://www.zlib.net/fossils/zlib-1.3.1.tar.gz"
SHA1SUM="f535367b1a11e2f9ac3bec723fb007fbc0d189e5"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    cd ${SRC_DIR}
    make distclean
    ./configure --uname=PhoenixOS || exit 1
    make -O -j${NPROC} || exit 1
}

function install()
{
    cp -P --preserve=links ${SRC_DIR}/lib* ${SYSROOT}/lib || exit 1
}
