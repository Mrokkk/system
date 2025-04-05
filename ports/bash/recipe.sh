REPO="https://github.com/bminor/bash.git"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
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
        --disable-multibyte || exit 1

    sed -i 's/#define HAVE_DLCLOSE 1/\/* #undef HAVE_DLCLOSE *\//' ${BUILD_DIR}/config.h \
        || exit 1

    sed -i 's/#define HAVE_DLOPEN 1/\/* #undef HAVE_DLOPEN *\//' ${BUILD_DIR}/config.h \
        || exit 1

    sed -i 's/#define HAVE_DLSYM 1/\/* #undef HAVE_DLSYM *\//' ${BUILD_DIR}/config.h \
        || exit 1

    make bash -j${NPROC} || exit 1
}

function install()
{
    make -O install -j${NPROC} || exit 1
}
