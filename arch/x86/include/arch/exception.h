#ifndef __EXCEPTION_H_
#define __EXCEPTION_H_

#define __NR_divide_error           0
#define __NR_debug                  1
#define __NR_nmi                    2
#define __NR_breakpoint             3
#define __NR_overflow               4
#define __NR_bound_range            5
#define __NR_invalid_opcode         6
#define __NR_device_na              7
#define __NR_double_fault           8
#define __NR_coprocessor            9
#define __NR_invalid_tss            10
#define __NR_segment_np             11
#define __NR_stack_segment          12
#define __NR_general_protection     13
#define __NR_page_fault             14

#define __MNE_divide_error          DE
#define __MNE_debug                 DB
#define __MNE_nmi
#define __MNE_breakpoint            BP
#define __MNE_overflow              OE
#define __MNE_bound_range           BR
#define __MNE_invalid_opcode        UD
#define __MNE_device_na             NM
#define __MNE_double_fault          DF
#define __MNE_coprocessor
#define __MNE_invalid_tss           TS
#define __MNE_segment_np            NP
#define __MNE_stack_segment         SS
#define __MNE_general_protection    GP
#define __MNE_page_fault            PF

#define __STRING_divide_error       "Division By Zero"
#define __STRING_debug              "Debug"
#define __STRING_nmi                "Non Maskable Interrupt"
#define __STRING_breakpoint         "Breakpoint"
#define __STRING_overflow           "Into Detected Overflow"
#define __STRING_bound_range        "Out of Bounds"
#define __STRING_invalid_opcode     "Invalid Opcode"
#define __STRING_device_na          "No Coprocessor"
#define __STRING_double_fault       "Double Fault"
#define __STRING_coprocessor        "Coprocessor Segment Overrun"
#define __STRING_invalid_tss        "Bad TSS"
#define __STRING_segment_np         "Segment Not Present"
#define __STRING_stack_segment      "Stack Fault"
#define __STRING_general_protection "General Protection Fault"
#define __STRING_page_fault         "Page Fault"

#define __SIGNAL_divide_error       SIGKILL
#define __SIGNAL_debug              SIGKILL
#define __SIGNAL_nmi                SIGKILL
#define __SIGNAL_breakpoint         SIGKILL
#define __SIGNAL_overflow           SIGKILL
#define __SIGNAL_bound_range        SIGKILL
#define __SIGNAL_invalid_opcode     SIGKILL
#define __SIGNAL_device_na          SIGKILL
#define __SIGNAL_double_fault       SIGKILL
#define __SIGNAL_coprocessor        SIGKILL
#define __SIGNAL_invalid_tss        SIGKILL
#define __SIGNAL_segment_np         SIGKILL
#define __SIGNAL_stack_segment      SIGKILL
#define __SIGNAL_general_protection SIGKILL
#define __SIGNAL_page_fault         SIGKILL

#endif

#ifndef __exception_noerrno
#define __exception_noerrno(x)
#endif

#ifndef __exception_errno
#define __exception_errno(x)
#endif

#ifndef __exception_debug
#define __exception_debug(x)
#endif

__exception_noerrno(divide_error)
__exception_debug(debug)
__exception_noerrno(nmi)
__exception_noerrno(breakpoint)
__exception_noerrno(overflow)
__exception_noerrno(bound_range)
__exception_noerrno(invalid_opcode)
__exception_noerrno(device_na)
__exception_errno(double_fault)
__exception_noerrno(coprocessor)
__exception_errno(invalid_tss)
__exception_errno(segment_np)
__exception_errno(stack_segment)
__exception_errno(general_protection)
__exception_errno(page_fault)
