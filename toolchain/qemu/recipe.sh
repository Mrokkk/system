REPO="https://gitlab.com/qemu-project/qemu.git"
VERSION=
BRANCH="v9.2.0-rc3"
OPTIONAL=true

function build()
{
    export CFLAGS="-Wno-error"
    export CXXFLAGS="-Wno-error"

    if [[ ! -f ".ninja_deps" ]]
    then
        "${SRC_DIR}"/configure \
            --prefix="${PREFIX}" \
            --target-list=i386-softmmu,x86_64-softmmu || exit 1
    fi
    ninja all || exit 1
}

function install()
{
    ninja install || exit 1
}
