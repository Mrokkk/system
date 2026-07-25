REPO="https://git.savannah.gnu.org/git/findutils.git"
REVISION="2be6812e01df4350da25c8e9c63a938d45cdc136"

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
