REPO="https://ftpmirror.gnu.org/gnu/less/less-643.tar.gz"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
        --with-regex=none \
        --disable-xattr \
        --disable-libsmack \
        --disable-libcap || exit 1

    sed -i 's/#define HAVE_WCTYPE 1/\/* #undef HAVE_WCTYPE *\//' ${BUILD_DIR}/defines.h \
        || exit 1
    touch ${BUILD_DIR}/stamp-h

    make -O -j ${NPROC} || exit 1
}

function install()
{
    make install || exit 1
}
