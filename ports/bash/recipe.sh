REPO="https://github.com/bminor/bash.git"
VERSION=
BRANCH=

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
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
        --disable-multibyte

    make bash -j${NPROC} || die "compilation failed"
}

function install()
{
    make -O install -j${NPROC}
}
