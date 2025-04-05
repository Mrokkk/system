#!/bin/bash

# TODO: rewrite this in python

base_dir=$(dirname `readlink -f "$0"`)

. "${base_dir}/utils.sh"

target="i686-pc-phoenix"
#target="x86_64-pc-phoenix"
input_dir="$(readlink -f ${base_dir}/..)"
output_dir="${PWD}"
prefix="${PWD}/native-sysroot"
sysroot="${PWD}/sysroot"
nproc=$(nproc)
input=()
dry_run=
rebuild=

function _recipe_read()
{
    local recipe="${input_dir}/${PKG}/recipe.sh"
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

    if [[ ! -d "${SRC_DIR}" ]]
    then
        info "${PKG}: downloading ${REPO}..."

        if [[ "${REPO}" == *".git"* ]]
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
        elif [[ "${REPO}" == *"tar.gz"* ]]
        then
            local file="$(basename ${REPO})"
            wget ${REPO} -O "${file}"
            tar xf "${file}"
            mv "$(basename -- "${file}" .tar.gz)" "${SRC_DIR}"
            rm "${file}"
        elif [[ "${REPO}" == *".zip"* ]]
        then
            local file="$(basename ${REPO})"
            mkdir -p "${SRC_DIR}"
            pushd_silent "${SRC_DIR}"
            wget "${REPO}" -O "/tmp/${file}"
            unzip "/tmp/${file}"
            popd_silent
            rm "/tmp/${file}"
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

    (SRC_DIR=${SRC_DIR} \
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
    export NULL_DIR="${null_dir}"
    export CONF_DIR="${input_dir}/${PKG}"
    export SRC_DIR="${output_dir}/${PKG}"
    export NATIVE_SYSROOT="${prefix}"
    export BUILD_DIR="${output_dir}/$(dirname ${PKG})/build/$(basename ${PKG})"

    debug "${PKG}: CONF_DIR:  ${CONF_DIR}"
    debug "${PKG}: SRC_DIR:   ${SRC_DIR}"
    debug "${PKG}: BUILD_DIR: ${BUILD_DIR}"

    [[ -n "${rebuild}" ]] && rm -rf "${BUILD_DIR}"
    create_dir "${BUILD_DIR}"

    (_recipe_read; \
    _prepare; \
    _step_execute build; \
    _step_execute install; \
    _recipe_close) || exit

    unset PKG SRC_DIR BUILD_DIR
}

function _args_parse()
{
    while [[ $# -gt 0 ]]; do
        arg="$1"
        case $arg in
            -d|--dbg)
                set -x ;;
            -v|--verbose)
                verbose=1 ;;
            -n|--dry-run)
                dry_run=1 ;;
            -f|--rebuild)
                rebuild=1 ;;
            *)
                input+=("${1}") ;;
        esac
        shift
    done
}

function main()
{
    _args_parse "${@}"
    info "Native sysroot: ${prefix}"
    info "Sysroot:        ${sysroot}"
    info "Input dir:      ${input_dir}"
    info "Output dir:     ${output_dir}"

    create_dir "${prefix}"
    create_dir "${sysroot}/usr/include"
    create_dir "${sysroot}/usr/lib"
    copy_dir_content "${base_dir}/../lib/include" "${sysroot}/usr/include"
    copy_dir_content "${base_dir}/../common/include" "${sysroot}/usr/include"
    copy_dir_content "${base_dir}/../include" "${sysroot}/usr/include"

    local packages=()
    declare -A forced_packages

    pushd_silent "${input_dir}"
    if [[ ${#input[@]} -eq 0 ]]
    then
        pushd_silent "${input_dir}"
        for recipe in "toolchain/*/recipe.sh"
        do
            packages+=($(dirname "${recipe}"))
        done
        for recipe in "ports/*/recipe.sh"
        do
            packages+=($(dirname "${recipe}"))
        done
    else
        for x in "${input[@]}"
        do
            if [[ -f "${x}/recipe.sh" ]]
            then
                packages+=("${x}")
                forced_packages["${x}"]=true
            else
                for recipe in $(compgen -G "${x}/*/recipe.sh" | sort)
                do
                    packages+=($(dirname "${recipe}"))
                done
            fi
        done
    fi
    popd_silent

    local pkgs_to_build=()

    for pkg in "${packages[@]}"
    do
        PKG=${pkg} _recipe_read
        if [[ -n "${OPTIONAL}" ]] && [[ -z "${forced_packages[${pkg}]}" ]]
        then
            debug "Ignoring optional package: ${pkg}"
        else
            debug "Selecting for build: ${pkg}"
            pkgs_to_build+=("${pkg}")
        fi
        _recipe_close
    done

    info "Building: ${pkgs_to_build[*]}"

    for pkg in "${pkgs_to_build[@]}"
    do
        if [[ -z "${dry_run}" ]]
        then
            _pkg_make "${pkg}"
        else
            info "${pkg}"
        fi
    done

    cleanup
}

main "${@}"
