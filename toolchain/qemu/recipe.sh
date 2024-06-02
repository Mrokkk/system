REPO="https://gitlab.com/qemu-project/qemu.git"
VERSION=
BRANCH=
OPTIONAL=true

function build()
{
    if [[ ! -f ".ninja_deps" ]]
    then
        "${SRC_DIR}"/configure \
            --prefix="${PREFIX}" \
            --target-list=i386-softmmu || die "configuration failed"
    fi
    ninja all || die "compilation failed"
}

function install()
{
    ninja install || die "installation failed"
}
