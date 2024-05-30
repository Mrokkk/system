REPO="https://gitlab.com/qemu-project/qemu.git"
VERSION=
BRANCH=
OPTIONAL=true

function build()
{
    "${SRC_DIR}"/configure \
        --prefix="${PREFIX}" \
        --target-list=i386-softmmu || die "configuration failed"
    ninja all || die "compilation failed"
}

function install()
{
    ninja install || die "installation failed"
}
