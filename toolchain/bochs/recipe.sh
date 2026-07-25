REPO="https://github.com/bochs-emu/Bochs.git"
BRANCH="REL_3_0_FINAL"
REVISION="02c851a93572071578d15f68c5701676a109dd19"
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
            --with-sdl2 || exit 1
    fi

    make -j${NPROC} || exit 1
}

function install()
{
    make install -j${NPROC} || exit 1
}
