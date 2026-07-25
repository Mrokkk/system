REPO="https://github.com/Mrokkk/fbDOOM.git"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    if [[ ! -f "Makefile" ]]
    then
        cmake -DCMAKE_INSTALL_PREFIX="${SYSROOT}" "${SRC_DIR}" || exit 1
    fi
    make -j${NPROC} || exit 1
}

function install()
{
    mkdir -p "${SYSROOT}/usr/share/doom"
    rsync -a --checksum "${SRC_DIR}/default.cfg" "${SYSROOT}/usr/share/doom"
    make install -j${NPROC} || exit 1
}
