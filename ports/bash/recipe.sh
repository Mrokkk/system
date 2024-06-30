REPO="https://github.com/bminor/bash.git"
VERSION=
BRANCH=

function build()
{
    local cross_gcc="${NATIVE_SYSROOT}/bin/i686-pc-phoenix-gcc"
    export CC="ccache ${cross_gcc}"
    export CFLAGS="-fdiagnostics-color=always -ggdb3 -fPIC"
    export PATH="${NATIVE_SYSROOT}/bin:${PATH}"

    [[ ! -f "${cross_gcc}" ]] && die "Toolchain not built"

    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix=${SYSROOT} \
            --host=i686-pc-phoenix \
            --target=i686-pc-phoenix \
            --enable-minimal-config \
            --enable-alias \
            --enable-job-control \
            --enable-prompt-string-decoding \
            --enable-directory-stack \
            --enable-extended-glob \
            --enable-cond-command \
            --enable-cond-regexp \
            --enable-command-timing \
            --enable-array-variables \
            --enable-brace-expansion \
            --enable-arith-for-command \
            --enable-casemod-attributes \
            --enable-casemod-expansions \
            --enable-process-substitution \
            --enable-readline \
            --disable-largefile \
            --disable-nls \
            --disable-threads \
            --disable-multibyte \
            --infodir="${NULL_DIR}" \
            --localedir="${NULL_DIR}" \
            --mandir="${NULL_DIR}" \
            --docdir="${NULL_DIR}" \
            --htmldir="${NULL_DIR}" \
            || die "configuration failed"
    fi

    make bash -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O install -j${NPROC}
}
