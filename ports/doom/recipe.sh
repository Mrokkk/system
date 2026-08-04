REPO="https://github.com/Mrokkk/fbDOOM.git"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    if [[ ! -f "Makefile" ]] || [[ ! -f CMakeCache.txt ]]
    then
        cmake -DCMAKE_INSTALL_PREFIX="${SYSROOT}" -DCMAKE_INSTALL_DATADIR="${SYSROOT}/usr/share/doom" -DDOOM_DATA_DIR="/usr/share/doom" "${SRC_DIR}" || exit 1
    fi
    make -j${NPROC} || exit 1
}

function install()
{
    mkdir -p "${SYSROOT}/usr/share/doom"
    rsync -a --checksum "${SRC_DIR}/default.cfg" "${SYSROOT}/usr/share/doom"
    make install -j${NPROC} || exit 1
}
