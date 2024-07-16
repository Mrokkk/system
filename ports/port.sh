export CROSS_GCC="${NATIVE_SYSROOT}/bin/i686-pc-phoenix-gcc"
export CC="ccache ${CROSS_GCC}"
export CFLAGS="-Wno-error -fdiagnostics-color=always -ggdb3 -fPIC"
export PATH="${NATIVE_SYSROOT}/bin:${PATH}"

[[ ! -f "${CROSS_GCC}" ]] && die "Toolchain not built"

function gnu_configuration()
{
    if [[ ! -f "${SRC_DIR}/configure" ]]
    then
        pushd_silent "${SRC_DIR}"
        ${SRC_DIR}/bootstrap --skip-po
        popd_silent
    fi

    if [[ -d "${SRC_DIR}/gnulib" ]]
    then
        pushd_silent "${SRC_DIR}/gnulib"
        if git diff --quiet
        then
            patch -p1 < "${CONF_DIR}/../gnulib/gnulib.patch"
        fi
        popd_silent
    fi

    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix=${SYSROOT} \
            --host=i686-pc-phoenix \
            --target=i686-pc-phoenix \
            --disable-nls \
            --disable-threads \
            --disable-acl \
            --infodir="${NULL_DIR}" \
            --localedir="${NULL_DIR}" \
            --mandir="${NULL_DIR}" \
            --docdir="${NULL_DIR}" \
            --htmldir="${NULL_DIR}" \
            ${@}|| die "configuration failed"
    fi
}
