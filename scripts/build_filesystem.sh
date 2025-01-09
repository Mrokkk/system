#!/bin/bash

base_dir=$(dirname $0)

. "${base_dir}/utils.sh"

disk_image="disk.img"
disk_size=300
mountpoint=mnt
boot_dir="${mountpoint}/boot"
grub_cfg="${boot_dir}/grub/grub.cfg"
dev=""
create_partition=
mount_loopback=
rebuild=
font=/usr/share/kbd/consolefonts/Lat2-Terminus16.psfu.gz
grub=$(command -v grub-install 2>/dev/null) || die "No grub-install"

while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        --rebuild)
            mount_loopback=1
            rebuild=1 ;;
        -v|--verbose)
            verbose=1 ;;
        *)
            break ;;
    esac
    shift
done

if mountpoint -q ${mountpoint}
then
    dev=$(mount | grep "${PWD}/${mountpoint}" | awk '{print $1;}' | sed -e 's/p[0-9]$//')
    if [[ ! -z "${rebuild}" ]]
    then
        execute_cmd "unmounting previous file system... " sudo umount "${mountpoint}"
        rm -rf "${mountpoint}"
        if [[ ! -z "${dev}" ]]
        then
            execute_cmd "removing loopback ${dev}... " sudo losetup -d "${dev}"
            dev=
        fi
    fi
else
    mount_loopback=1
fi

if [[ ! -f ${disk_image} ]] || [[ ! -z "${rebuild}" ]]
then
    create_partition=1

    rm -rf "${disk_image}"
    execute_cmd "creating new image... " dd if=/dev/zero of="${disk_image}" bs=1M count="${disk_size}" status=none
fi

if [[ -z "${dev}" ]]
then
    execute_cmd "creating loopback... " dev=$(sudo losetup --find --partscan --show "${disk_image}")
fi

echo "loopback device: ${dev}"

partition_number="p1"

if [[ ! -z "${create_partition}" ]]
then
    execute_cmd "creating ext2 partition... "   sudo parted -s "${dev}" mklabel msdos mkpart primary ext2 1MiB 100%
    execute_cmd "destroying old filesystem... " sudo dd if=/dev/zero of="${dev}${partition_number}" bs=1M count=1 status=none
    execute_cmd "creating new filesystem... "   sudo mke2fs -q -I 128 "${dev}${partition_number}"
fi

if [[ ! -z "${mount_loopback}" ]]
then
    mkdir -p mnt
    execute_cmd "mounting filesystem... "       sudo mount -t ext2 -o rw "${dev}${partition_number}" ${mountpoint}

    sudo chown -R ${USER} ${mountpoint}
    sudo chgrp -R ${USER} ${mountpoint}
fi

if [[ ! -f "font.psf" ]] || [[ "${0}" -nt "font.psf" ]] || [[ -n ${rebuild} ]]
then
    rm -f font.psf
    cp ${font} font.psf.gz
    gzip -d font.psf.gz
fi

inputrc_content='set bell-style none

set meta-flag on
set input-meta on
set convert-meta off
set output-meta on

$if mode=emacs
"\e[1~": beginning-of-line
"\e[4~": end-of-line
"\e[5~": beginning-of-history
"\e[6~": end-of-history
"\e[7~": beginning-of-line
"\e[3~": delete-char
"\e[2~": quoted-insert
"\e[5C": forward-word
"\e[5D": backward-word
"\e\e[C": forward-word
"\e\e[D": backward-word
"\e[1;5C": forward-word
"\e[1;5D": backward-word
$endif'

bashrc_content='export PS1="\u \e[34m\w\e[0m # "

function path()
{
    stat -L "${1}" >/dev/null && readlink -f "${1}"
}

alias ktest=/bin/test
alias ..="cd .."
alias ls="ls --color=auto"
alias l="ls -lah"
alias dmesg="cat /proc/syslog"
alias f="find . -name"
alias p="path"
function k()
{
    while ktest -q -t realloc_small_to_small; do echo -n; done
}'

vimrc_content="let mapleader = \",\"

nmap <Leader>t :tabnew<CR>
nmap <Leader>l :tabn<CR>
nmap <Leader>h :tabp<CR>
nmap <Leader>q :bd<CR>
nmap <space> viw

syntax on
set noswapfile
set number
set cursorline
set nowrap
set softtabstop=4
set autoindent
set smartindent
set tabstop=4
set shiftwidth=4
set expandtab
set nocompatible
filetype off

set rtp+=~/.vim/bundle/Vundle.vim
call vundle#begin()
Plugin 'preservim/nerdtree'
Plugin 'sainnhe/gruvbox-material'
call vundle#end()
filetype plugin indent on

let g:NERDTreeDirArrowExpandable=\"+\"
let g:NERDTreeDirArrowCollapsible=\"-\"
let NERDTreeShowHidden = 0
let NERDTreeQuitOnOpen = 0
let NERDTreeAutoDeleteBuffer = 1
let NERDTreeMinimalUI = 1
nmap <Leader>e :NERDTreeToggle<CR>
nmap <Leader>f :NERDTreeFind<CR>

if has('termguicolors')
    set termguicolors
endif

set background=dark
let g:gruvbox_material_background = 'medium'
let g:gruvbox_material_better_performance = 1
let g:gruvbox_material_disable_italic_comment = 1
colorscheme gruvbox-material"

create_dir "${mountpoint}/bin"
create_dir "${mountpoint}/dev"
create_dir "${mountpoint}/lib"
create_dir "${mountpoint}/usr/share"
create_dir "${mountpoint}/lib/modules"
create_dir "${mountpoint}/tmp"
create_dir "${mountpoint}/proc"
create_dir "${mountpoint}/root"
create_dir "${mountpoint}/root/.vim"
create_dir "${mountpoint}/sys"
create_dir "${boot_dir}/grub"

for binary in $(find bin -type f -executable)
do
    if [[ ${binary} == *".so"* ]]
    then
        continue
    fi
    copy ${binary} ${mountpoint}/bin
done

copy_dir_content "modules" "${mountpoint}/lib/modules"

copy ../arch/x86/cpuid.c ${mountpoint}
copy ../tux.tga ${mountpoint}
copy ../cursor.tga ${mountpoint}
copy ../close.tga ${mountpoint}
copy ../close_pressed.tga ${mountpoint}
copy font.psf ${mountpoint}/usr/share
copy_dir_content sysroot/usr/share ${mountpoint}/usr/share
copy_dir_content sysroot/lib ${mountpoint}/lib
copy_dir_content sysroot/bin ${mountpoint}/bin

write_to "${bashrc_content}" "${mountpoint}/root/.bashrc"
write_to "${inputrc_content}" "${mountpoint}/root/.inputrc"
write_to "${vimrc_content}" "${mountpoint}/root/.vimrc"
copy kernel.map "${boot_dir}"
copy system "${boot_dir}"

if [[ ! -d "${mountpoint}/root/.vim/bundle" ]]
then
    create_dir "${mountpoint}/root/.vim/bundle"
    git clone https://github.com/VundleVim/Vundle.vim.git "${mountpoint}/root/.vim/bundle/Vundle.vim"
    git clone https://github.com/preservim/nerdtree.git "${mountpoint}/root/.vim/bundle/nerdtree"
    if [[ -d "~/.vim/bundle/gruvbox-material" ]] # FIXME: gruvbox-material writes files during first start
    then
        create_dir "${mountpoint}/root/.vim/bundle/gruvbox-material"
        copy_dir_content "~/.vim/bundle/gruvbox-material" "${mountpoint}/root/.vim/bundle/gruvbox-material"
    fi
fi

if [[ ! -d ${boot_dir}/grub/i386-pc ]]
then
    sudo ${grub} --boot-directory=${boot_dir} --target=i386-pc --modules="ext2 part_msdos" "${dev}"
fi

sync -f ${mountpoint}

cleanup
