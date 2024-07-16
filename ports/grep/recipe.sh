REPO="https://git.savannah.gnu.org/git/grep.git"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
        --disable-xattr \
        --disable-libsmack \
        --disable-libcap

    make -O -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O install -j${NPROC} || die "installation failed"
}
