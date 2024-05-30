#!/bin/bash

base_dir=$(dirname `readlink -f "$0"`)

. "${base_dir}/utils.sh"

target="i686-pc-phoenix"
toolchain_input_dir="$(readlink -f ${base_dir}/../toolchain)"
toolchain_output_dir="${PWD}/toolchain"
build_dir="${toolchain_output_dir}/build"
prefix="${PWD}/native-sysroot"
sysroot="${PWD}/sysroot"
nproc=$(nproc)
packages=()

while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        -v|--verbose)
            verbose=1 ;;
        *)
            packages+=("${1}")
            ;;
    esac
    shift
done

function _recipe_read()
{
    local recipe="${toolchain_input_dir}/${PKG}/recipe.sh"
    [[ -f "${recipe}" ]] || die "No ${recipe}"
    . ${recipe}
}

function _recipe_close()
{
    unset -f build install
    unset REPO VERSION BRANCH OPTIONAL
}

function _prepare()
{
    info "${PKG}: preparing..."

    create_dir "${BUILD_DIR}"

    if [[ ! -d "${SRC_DIR}" ]]
    then
        info "${PKG}: downloading ${REPO}..."

        if [[ "${REPO}" == *"git"* ]]
        then
            local branch_arg=""
            if [[ -n "${BRANCH}" ]]
            then
                branch_arg="--branch ${BRANCH}"
            fi
            git clone ${branch_arg} --depth 1 ${REPO} ${SRC_DIR}
        elif [[ "${REPO}" == *"svn"* ]]
        then
            svn checkout "${REPO}" "${SRC_DIR}"
        fi

        pushd_silent ${SRC_DIR}

        if [[ -d "${CONF_DIR}/patches" ]]
        then
            for f in "${CONF_DIR}"/patches/*
            do
                echo "patching with ${f}"
                patch -p1 < "${f}" || die "cannot patch"
            done
        fi

        popd_silent
    else
        debug "${PKG}: ${SRC_DIR} already exists"
    fi
}

function _step_execute()
{
    info "${PKG}: starting ${1}..."

    pushd_silent "${BUILD_DIR}"

    (set -eo pipefail; \
    SRC_DIR=${SRC_DIR} \
    PREFIX=${prefix} \
    TARGET=${target} \
    SYSROOT=${sysroot} \
    NPROC=${nproc} \
        ${1}) || die "${PKG}: ${1} failed"

    success "${PKG}: ${1} succeeded"

    popd_silent
}

function _pkg_make()
{
    export PKG="${1}"
    export CONF_DIR="${toolchain_input_dir}/${PKG}"
    export SRC_DIR="${toolchain_output_dir}/${PKG}"
    export BUILD_DIR="${build_dir}/${PKG}"
    _recipe_read

    if [[ -z "${OPTIONAL}" ]] || [[ -n "${2}" ]]
    then
        _prepare
        _step_execute build
        _step_execute install
    else
        info "${PKG}: ignoring; optional package"
    fi

    _recipe_close
    unset PKG SRC_DIR BUILD_DIR
}

info "Native sysroot: ${prefix}"
info "Sysroot:        ${sysroot}"

create_dir "${prefix}"
create_dir "${sysroot}/usr/include"
create_dir "${sysroot}/usr/lib"
copy_dir_content "${base_dir}/../lib/include" "${sysroot}/usr/include"
copy_dir_content "${base_dir}/../include" "${sysroot}/usr/include"

if [[ ${#packages[@]} -eq 0 ]]
then
    for component in ${toolchain_input_dir}/*
    do
        _pkg_make "$(basename ${component})"
    done
else
    for component in "${packages[@]}"
    do
        _pkg_make "${component}" "force"
    done
fi

cleanup
