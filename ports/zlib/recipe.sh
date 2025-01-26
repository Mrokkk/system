REPO="http://www.zlib.net/zlib-1.3.1.tar.gz"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    cd ${SRC_DIR}
    make distclean
    ./configure --uname=PhoenixOS || die "configuration failed"
    make -O -j${NPROC} || die "compilation failed"
}

function install()
{
    cp -P --preserve=links ${SRC_DIR}/lib* ${SYSROOT}/lib
}
