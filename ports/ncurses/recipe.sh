REPO="https://github.com/mirror/ncurses.git"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration_raw \
        --disable-xattr \
        --disable-libsmack \
        --disable-libcap \
        --with-shared \
        --without-dlsym \
        --without-ada \
        --enable-sigwinch \
        --with-ticlib \
        --without-cxx \
        --without-cxx-binding \
        --without-tests \
        --without-manpages

    make -O -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O -j${NPROC} install || die "installation failed"
}
