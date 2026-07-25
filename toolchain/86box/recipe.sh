REPO="https://github.com/86Box/86Box.git"
BRANCH="v6.0"
REVISION="4fef696a4eead1d55a28d6ac0e5bd2864e5454da"
OPTIONAL=true

function build()
{
    cmake "${SRC_DIR}" --preset optimized -D CMAKE_TOOLCHAIN_FILE="./cmake/flags-gcc-x86_64.cmake" -D DISCORD=OFF || exit 1
    cmake --build . -- -j${NPROC} || exit 1
}

function install()
{
    cmake --install . --prefix "${PREFIX}" || exit 1
    if [[ ! -d ${PREFIX}/bin/roms ]]
    then
        git clone --depth 1 --branch "v6.0" https://github.com/86Box/roms.git ${PREFIX}/bin/roms || exit 1
    fi
}
