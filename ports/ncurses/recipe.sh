REPO="https://github.com/mirror/ncurses.git"
VERSION=
BRANCH="v6.4"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration_raw \
        --program-prefix="" \
        --datarootdir="${SYSROOT}/usr/share" \
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
        --without-manpages \
        --disable-stripping || exit 1

    sed -i 's/^.* TERMINFO_DIRS .*/#define TERMINFO_DIRS \"\/usr\/share\/terminfo\"/g;
            s/^.* TERMINFO .*/#define TERMINFO_DIRS \"\/usr\/share\/terminfo\"/g' \
        ${BUILD_DIR}/include/ncurses_cfg.h || exit 1

    make -O -j${NPROC} || exit 1
}

function install()
{
    make -O -j${NPROC} install || exit 1
    rm ${SYSROOT}/usr/lib/terminfo
}
