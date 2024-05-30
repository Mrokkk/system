REPO="https://svn.code.sf.net/p/bochs/code/trunk/bochs"
VERSION=
BRANCH=
OPTIONAL=true

function build()
{
    if [[ ! -f "Makefile" ]]
    then
        ${SRC_DIR}/configure \
            --prefix="${PREFIX}" \
            --enable-smp \
            --enable-cpu-level=6 \
            --enable-all-optimizations \
            --enable-x86-64 \
            --enable-pci \
            --enable-vmx \
            --enable-voodoo \
            --enable-debugger \
            --enable-debugger-gui \
            --enable-logging \
            --enable-fpu \
            --enable-3dnow \
            --enable-cdrom \
            --enable-x86-debugger \
            --enable-iodebug \
            --disable-plugins \
            --disable-docbook \
            --enable-idle-hack \
            --with-x \
            --with-x11 \
            --with-term \
            --with-sdl2 || die "bosch configuration failed"
    fi

    make -j${NPROC} || die "bosch compilation failed"
}

function install()
{
    make install -j${NPROC} || die "bochs installation failed"
}
