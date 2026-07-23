REPO="https://github.com/Akbar30Bill/DOOM_wads.git"
REVISION=9b384dc68add3eb2f5eb7754654cafeeaea5103b

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    :
}

function install()
{
    mkdir -p "${SYSROOT}/usr/share/doom" || exit 1
    rsync -a --checksum ${SRC_DIR}/doom.wad "${SYSROOT}/usr/share/doom" || exit 1
    rsync -a --checksum ${SRC_DIR}/doom2.wad "${SYSROOT}/usr/share/doom" || exit 1
}
