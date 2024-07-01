#pragma once

#include <kernel/vm.h>
#include <kernel/printk.h>

void vm_file_path_read(vm_area_t* vma, char* buffer, size_t max_len);

void vm_area_log_internal(
    const printk_entry_t* entry,
    vm_area_t* vma,
    int indent_lvl);

void vm_areas_log_internal(
    const printk_entry_t* entry,
    vm_area_t* vm_areas,
    int indent_lvl,
    const char* header_fmt,
    ...);

static inline char* vm_flags_string(char* buffer, int vm_flags)
{
    char* b = buffer;
    *b++ = (vm_flags & VM_READ) ? 'r' : '-';
    *b++ = (vm_flags & VM_WRITE) ? 'w' : '-';
    *b++ = (vm_flags & VM_EXEC) ? 'x' : '-';
    *b++ = (vm_flags & VM_IO) ? 'i' : '-';
    *b = '\0';
    return buffer;
}

#define INDENT_LVL_0    0
#define INDENT_LVL_1    1
#define INDENT_LVL_2    2
#define INDENT_LVL_3    3
#define INDENT_LVL_4    4

#define vm_area_log_debug(flag, vma) \
    do \
    { \
        if (flag) vm_area_log(KERN_DEBUG, vma); \
    } \
    while (0)

#define vm_area_log(severity, vma) \
    do \
    { \
        PRINTK_ENTRY(__e, severity); \
        vm_area_log_internal(&__e, vma, INDENT_LVL_0); \
    } \
    while (0)

#define vm_areas_indent_log(severity, vm_areas, indent, header_fmt, ...) \
    do \
    { \
        PRINTK_ENTRY(__e, severity); \
        vm_areas_log_internal(&__e, vm_areas, indent, header_fmt, ##__VA_ARGS__); \
    } \
    while (0)

#define vm_areas_log(severity, vm_areas, header_fmt, ...) \
    do \
    { \
        PRINTK_ENTRY(__e, severity); \
        vm_areas_log_internal(&__e, vm_areas, INDENT_LVL_0, header_fmt, ##__VA_ARGS__); \
    } \
    while (0)

#define vm_areas_log_debug(flag, ...) \
    do \
    { \
        if (flag) vm_areas_log(KERN_DEBUG, __VA_ARGS__); \
    } \
    while (0)

#define process_vm_areas_log(severity, process) \
    vm_areas_log(severity, (process)->mm->vm_areas, "%s[%u] vm areas:", \
        (process)->name, \
        (process)->pid)

#define process_vm_areas_indent_log(severity, process, indent) \
    vm_areas_indent_log(severity, (process)->mm->vm_areas, indent, "%s[%u] vm areas:", \
        (process)->name, \
        (process)->pid)
