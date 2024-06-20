#!/bin/bash

stderr=
verbose=
modification_list=
RED="\e[31m"
GREEN="\e[32m"
BLUE="\e[34m"
ERROR="[ ERROR   ]"
SUCCESS="[ SUCCESS ]"
INFO="[  INFO   ]"
DEBUG="[  DEBUG  ]"
RESET="\e[0m"
null_dir="/tmp/nulldir"

function cleanup()
{
    if [[ -n "${modification_list}" ]]
    then
        if [[ -n "${verbose}" ]]
        then
            echo "Removing ${modification_list}"
        fi
        rm -rf "${modification_list}"
    fi
    rm -rf "${null_dir}"
}

function any_file_changed()
{
    if [[ -n "${modification_list}" ]]
    then
        if [[ -s "${modification_list}" ]]
        then
            return 0
        else
            return 1
        fi
    else
        return 1
    fi
}

function create_modification_list()
{
    if [[ -z "${modification_list}" ]]
    then
        modification_list=$(mktemp)
        if [[ -n "${verbose}" ]]
        then
            echo "Created ${modification_list}"
        fi
    fi
}

function binary_from_native_sysroot()
{
    [[ -f "native-sysroot/bin/${1}" ]] && echo $(readlink -f "native-sysroot/bin/${1}")
}

function resource_from_native_sysroot()
{
    local file="$(find native-sysroot -name ${1} | head -n 1)"
    if [[ -n "${file}" ]]
    then
        echo $(readlink -f "${file}")
    fi
}

function display_file()
{
    local file="${1}"
    cat "${stderr}" | sed 's/^/>>>> /'
}

function info()
{
    echo -e "${BLUE}${INFO}${RESET} ${@}"
}

function debug()
{
    if [[ -n "${verbose}" ]]
    then
        echo -e "${BLUE}${INFO}${RESET} ${@}"
    fi
}

function success()
{
    echo -e "${GREEN}${SUCCESS}${RESET} ${@}"
}

function die()
{
    echo -e "${RED}${ERROR} ${@}${RESET}"
    if [[ -f ${stderr} ]]
    then
        echo "Error:"
        display_file ${stderr}
    fi
    cleanup
    exit -1
}

function pushd_silent()
{
    pushd "${1}" &>/dev/null || die "No directory: ${1}"
}

function popd_silent()
{
    popd &>/dev/null || die "Cannot go back to previous dir!"
}

function execute_cmd()
{
    local cmd="${@:2}"
    stderr=$(mktemp)
    echo -n "${1}"
    echo "Command: ${cmd}">"${stderr}"
    eval ${cmd} &>>"${stderr}" || die "failed"
    echo "done"
    if [[ ! -z "${verbose}" ]]
    then
        display_file "${stderr}"
    fi
    if [[ ! -z "${stderr}" ]]
    then
        rm -rf "${stderr}"
    fi
}

function create_dir()
{
    local dest="${1}"
    if [[ ! -d "${dest}" ]]
    then
        execute_cmd "creating ${dest}... " mkdir -p "${dest}"
    fi
}

function copy()
{
    local src="${1}"
    local dest="${2}/$(basename ${src})"
    if ! cmp -s "${src}" "${dest}"
    then
        execute_cmd "copying ${src} to ${dest}... " rsync "${src}" "${dest}"
        create_modification_list
        echo "${dest}" >> "${modification_list}"
    else
        echo "${dest}: up to date"
    fi
}

function copy_dir_content()
{
    local src="${1}"
    local dest="${2}"
    create_modification_list
    rsync -ai --checksum -r "${src}/" "${dest}" | grep -E '^>' >>"${modification_list}"
}

function write_to()
{
    local content="${1}"
    local dest="${2}"
    stderr=$(mktemp)

    if [[ -f "${dest}" ]]
    then
        existing_content=$(cat "${dest}" 2>/dev/null)
        if [[ "${content}" == "${existing_content}" ]]
        then
            echo "${dest}: up to date"
            rm -rf "${stderr}"
            return
        fi
    fi

    echo -n "writing to ${dest}... "
    echo "${content}" 1>"${dest}" 2>"${stderr}" || die "failed"

    create_modification_list
    echo "${dest}" >>"${modification_list}"
    echo "done"
    rm -rf "${stderr}"
}
