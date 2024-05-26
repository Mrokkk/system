#!/bin/bash

stderr=
verbose=
modification_list=

function cleanup()
{
    if [[ -n "${modification_list}" ]]
    then
        [[ -n "${verbose}" ]] && echo "Removing ${modification_list}"
        rm -rf "${modification_list}"
    fi
}

function any_change_done()
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
        [[ -n "${verbose}" ]] && echo "Created ${modification_list}"
    fi
}

function display_file()
{
    local file="${1}"
    cat "${stderr}" | sed 's/^/>>>> /'
}

function die()
{
    echo "${@}"
    if [[ -f ${stderr} ]]
    then
        echo "Error:"
        display_file ${stderr}
    fi
    cleanup
    exit -1
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
    [[ ! -d "${dest}" ]] && execute_cmd "creating ${dest}... " mkdir -p "${dest}"
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
