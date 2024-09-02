.global vsyscall
vsyscall:
    mov 4(%esp), %eax
    push %edi
    push %esi
    push %edx
    push %ecx
    push %ebx
    push %ebp
    call __x86.get_pc_thunk.bx
    addl $_GLOBAL_OFFSET_TABLE_, %ebx
    mov 1f@GOT(%ebx), %edx
    mov %esp, %ecx
    sysenter

1:
    pop %ebp
    pop %ebx
    pop %ecx
    pop %edx
    pop %esi
    pop %edi
    ret
    .type vsyscall, @function
    .size vsyscall, .-vsyscall
