REPO="https://github.com/coreutils/coreutils.git"
VERSION=
BRANCH=
APPS=("ls" "tty" "dirname" "basename" "env" "fold" "sleep" "stty" "kill" "stat" "dircolors" "du" "uniq" "sort" "date" "readlink")

[[ -n "${CONF_DIR}" ]] && . ${CONF_DIR}/../port.sh

function build()
{
    gnu_configuration \
        --disable-largefile \
        --disable-xattr \
        --disable-libsmack \
        --disable-libcap \
        --enable-no-install-program=arch,coreutils,hostname,chcon,runcon,df,pinky,users,who,nice || exit 1

    make -O -j${NPROC}

    for app in "${APPS[@]}"
    do
        make -O src/${app} -j${NPROC} || exit 1
    done
}

function install()
{
    for app in "${APPS[@]}"
    do
        cp src/${app} ${SYSROOT}/bin
    done
}
