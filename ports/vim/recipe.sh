REPO="https://github.com/vim/vim.git"
VERSION=
BRANCH="v9.1.0983"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    export vim_cv_getcwd_broken=no
    export vim_cv_memmove_handles_overlap=yes
    export vim_cv_stat_ignores_slash=yes
    export vim_cv_tgetent=zero
    export vim_cv_terminfo=yes
    export vim_cv_toupper_broken=no
    export vim_cv_tty_group=world
    export vim_cv_ipv4_networking=no
    export vim_cv_ipv6_networking=no
    export vim_cv_uname_output='PhoenixOS'
    export vim_cv_uname_r_output='1.0-dev'
    export vim_cv_uname_m_output="i686"
    export ac_cv_prog_STRIP="/bin/true"

    cd "${SRC_DIR}"
    ./configure \
        --prefix=${SYSROOT} \
        --host=i686-pc-phoenix \
        --target=i686-pc-phoenix \
        --datarootdir="${SYSROOT}/usr/share" \
        --disable-selinux \
        --disable-darwin \
        --disable-smack \
        --disable-xattr \
        --disable-xsmp \
        --disable-xsmp-interact \
        --disable-gtktest \
        --disable-gui \
        --disable-largefile \
        --disable-acl \
        --disable-nls \
        --disable-netbeans \
        --disable-rightleft \
        --disable-arabic \
        --disable-icon-cache-update \
        --disable-canberra \
        --disable-libsodium \
        --with-features=tiny

    sed -i 's#default_vim_dir.*#default_vim_dir = (char*)"/usr/share/vim";#g' src/auto/pathdef.c || die "cannot fix pathdef.c"

    make -O -j4 || die "compilation failed"
}

function install()
{
    cd "${SRC_DIR}"
    make install
}
