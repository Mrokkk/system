REPO="https://github.com/source-foundry/Hack/releases/download/v3.003/Hack-v3.003-ttf.zip"

function build()
{
    mkdir -p "${BUILD_DIR}/sfn"
    ${NATIVE_SYSROOT}/bin/sfnconv -U -t m -M 128 "${SRC_DIR}/ttf/Hack-Regular.ttf" "${BUILD_DIR}/sfn/Hack-Regular.sfn"
    ${NATIVE_SYSROOT}/bin/sfnconv -U -t mb -M 128 "${SRC_DIR}/ttf/Hack-Bold.ttf" "${BUILD_DIR}/sfn/Hack-Bold.sfn"
    ${NATIVE_SYSROOT}/bin/sfnconv -U -t mi -M 128 "${SRC_DIR}/ttf/Hack-Italic.ttf" "${BUILD_DIR}/sfn/Hack-Italic.sfn"
}

function install()
{
    mkdir -p "${SYSROOT}/usr/share/fonts"
    cp -r "${BUILD_DIR}/sfn" "${SYSROOT}/usr/share/fonts"
}
