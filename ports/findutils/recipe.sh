REPO="https://git.savannah.gnu.org/git/findutils.git"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
        --disable-acl \
        --disable-xattr \
        --disable-libsmack \
        --disable-libcap \
        --disable-largefile

    make -O -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O -j${NPROC} install || die "installation failed"
}
