REPO="https://github.com/source-foundry/Hack/releases/download/v3.003/Hack-v3.003-ttf.zip"

function build()
{
    mkdir -p "${BUILD_DIR}/sfn"
    ${NATIVE_SYSROOT}/bin/sfnconv -U -t m "${SRC_DIR}/ttf/Hack-Regular.ttf" "${BUILD_DIR}/sfn/Hack-Regular.sfn" || exit 1
    ${NATIVE_SYSROOT}/bin/sfnconv -U -t mb "${SRC_DIR}/ttf/Hack-Bold.ttf" "${BUILD_DIR}/sfn/Hack-Bold.sfn" || exit 1
    ${NATIVE_SYSROOT}/bin/sfnconv -U -t mi "${SRC_DIR}/ttf/Hack-Italic.ttf" "${BUILD_DIR}/sfn/Hack-Italic.sfn" || exit 1
}

function install()
{
    mkdir -p "${SYSROOT}/usr/share/fonts" || exit 1
    cp -r "${BUILD_DIR}/sfn" "${SYSROOT}/usr/share/fonts" || exit 1
}
