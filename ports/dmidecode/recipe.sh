REPO="https://github.com/mirror/dmidecode.git"
BRANCH="dmidecode-3-6"
REVISION="51b1ecc262e4d0a45994f7a736ca1ab77b10480b"

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
