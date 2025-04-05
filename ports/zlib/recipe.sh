REPO="http://www.zlib.net/zlib-1.3.1.tar.gz"

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
