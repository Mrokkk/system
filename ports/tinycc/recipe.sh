REPO="https://github.com/TinyCC/tinycc.git"
BRANCH="release_0_9_27"

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix="${SYSROOT}" \
            --cpu="i686" \
            --enable-cross \
            --disable-static \
            --tccdir="${SYSROOT}/lib/tcc" \
            --cross-prefix="i686-pc-phoenix-" \
            --triplet="i686-pc-phoenix" \
            --crtprefix="/lib" \
            --libpaths="/lib" \
            --elfinterp="/lib/ld.so" \
            --sysincludepaths="/usr/include:/lib/tcc/include" || exit 1
    fi

    make -O -j${NPROC} tcc || exit 1
}

function install()
{
    make -O -j${NPROC} install || exit 1
    cp ${SYSROOT}/lib/tcc/include/stddef.h ${SYSROOT}/lib/tcc/include/stdint.h || exit 1
}
