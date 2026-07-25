REPO="https://git.kernel.org/pub/scm/linux/kernel/git/legion/kbd.git"
BRANCH="v2.7.1"
REVISION="f2c0d3185b300780f4f69abe3ff7493ec7634f39"

function build()
{
    :
}

function install()
{
    mkdir -p "${SYSROOT}/usr/share/kbd"
    rsync -a --checksum -r "${SRC_DIR}/data/consolefonts" "${SYSROOT}/usr/share/kbd"
}
