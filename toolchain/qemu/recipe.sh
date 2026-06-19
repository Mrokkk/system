REPO="https://gitlab.com/qemu-project/qemu.git"
VERSION=
BRANCH="v11.0.1"
OPTIONAL=true

function build()
{
    export CFLAGS="-Wno-error"
    export CXXFLAGS="-Wno-error"

    if [[ ! -f ".ninja_deps" ]]
    then
        "${SRC_DIR}"/configure \
            --prefix="${PREFIX}" \
            --extra-cflags="${CFLAGS}" \
            --extra-cxxflags="${CXXFLAGS}" \
            --target-list=i386-softmmu,x86_64-softmmu || exit 1
    fi
    ninja all || exit 1
}

function install()
{
    ninja install || exit 1
}
