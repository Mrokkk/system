REPO="https://git.savannah.gnu.org/git/findutils.git"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
        --disable-largefile || exit 1

    make -O -j${NPROC} || exit 1
}

function install()
{
    make -O -j${NPROC} install || exit 1
}
