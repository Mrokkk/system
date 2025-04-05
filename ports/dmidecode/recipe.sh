REPO="https://github.com/mirror/dmidecode.git"
BRANCH="dmidecode-3-6"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    cd "${SRC_DIR}"
    make all -j${NPROC} || exit 1
}

function install()
{
    rsync -a --checksum "${SRC_DIR}/dmidecode" "${SYSROOT}/bin" || exit 1
    rsync -a --checksum "${SRC_DIR}/biosdecode" "${SYSROOT}/bin" || exit 1
    rsync -a --checksum "${SRC_DIR}/vpddecode" "${SYSROOT}/bin" || exit 1
}
