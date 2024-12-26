REPO="https://gitlab.com/qemu-project/qemu.git"
VERSION=
BRANCH="v9.2.0-rc3"
OPTIONAL=true

function build()
{
    if [[ ! -f ".ninja_deps" ]]
    then
        "${SRC_DIR}"/configure \
            --prefix="${PREFIX}" \
            --target-list=i386-softmmu,x86_64-softmmu || die "configuration failed"
    fi
    ninja all || die "compilation failed"
}

function install()
{
    ninja install || die "installation failed"
}
