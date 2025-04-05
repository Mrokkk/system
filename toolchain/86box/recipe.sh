REPO="https://github.com/86Box/86Box.git"
OPTIONAL=true

function build()
{
    cmake -B . -S "${SRC_DIR}" --preset optimized || exit 1
    cmake --build . -- -j4 || exit 1
}

function install()
{
    cmake --install . --prefix "${PREFIX}" || exit 1
    if [[ ! -d ${PREFIX}/bin/roms ]]
    then
        git clone --depth 1 https://github.com/86Box/roms.git ${PREFIX}/bin/roms || exit 1
    fi
}
