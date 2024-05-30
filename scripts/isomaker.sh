#!/usr/bin/env bash

base_dir=$(dirname $0)

. "${base_dir}/utils.sh"

modules=()

while [[ $# -gt 0 ]]; do
    arg="$1"
    case $arg in
        -i|--input)
            binary="$2"
            shift ;;
        -o|--output)
            name="$2"
            shift ;;
        --multiboot2)
            multiboot2=1 ;;
        --grub-use-serial)
            serial=1 ;;
        --args)
            args="$2"
            shift ;;
        --module)
            modules+=("$(readlink -e ${2})")
            shift ;;
        -v|--verbose)
            verbose=1
            shift ;;
        *)
            break ;;
    esac
    shift
done

if [[ "${binary}" == "" ]]; then
    binary="${name}"
fi

if [[ "${multiboot2}" ]]; then
    multiboot_command="multiboot2"
else
    multiboot_command="multiboot"
fi

iso_dir="${name}.d"
grub_cfg="${name}.d/boot/grub/grub.cfg"
iso="${name}.iso"

modules_load=""
for mod in "${modules[@]}"; do
    modules_load+="module /boot/$(basename ${mod}) $(basename ${mod})
    "
done

# For debug: set debug=all
menu_entry="set timeout=0
serial --unit=0 --speed=9600
terminal_input serial
terminal_output serial
set default=0
menuentry "${binary}" {
    if ! ${multiboot_command} /boot/${name} ${args}; then reboot; fi
    ${modules_load}
    boot
}"

create_dir ${name}.d/boot/grub

write_to "${menu_entry}" "${grub_cfg}"

for mod in "${modules[@]}"; do
    copy "${mod}" ${name}.d/boot
done

copy ${binary} ${name}.d/boot
copy_dir_content "mnt" "${iso_dir}"

if [[ -n "${verbose}" ]]
then
    echo "Changed files (${modification_list}):"
    cat "${modification_list}"
fi

if any_file_changed
then
    execute_cmd "creating ${iso}... " grub-mkrescue -o "${iso}" "${iso_dir}"
else
    echo "${iso}: up to date"
fi

cleanup
