%macro PUSHALL 0
    push rax
	push rbx
	push rcx
	push rdx
	push rdi
	push rsi
	push rbp
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
%endmacro

%macro POPALL 0
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rsi
	pop rdi
	pop rdx
	pop rcx
	pop rbx
	pop rax
%endmacro

[extern VecHandler]
%macro GENERIC_EXC 1
    PUSHALL
    mov rdi, %1
    lea rsi, [rsp+120]
    call VecHandler
    POPALL
    iretq
%endmacro

[extern SendEOI]
%macro GENERIC_EXC 1
    PUSHALL
    mov rdi, %1
    lea rsi, [rsp+120]
    lea rdx, [rsp+]
    call VecHandler
    call SendEOI
    POPALL
    pop
    iretq
%endmacro

%macro EXCEPTION_WITH_ERR 1
    PUSHALL
    mov rdi, %1
    lea rsi, [rsp+120]
    call VecHandler
    call SendEOI
    POPALL
    iretq
%endmacro

global VEC_DIVISION_BY_ZERO_HANDLER
DIVISION_BY_ZERO_HANDLER:
    GENERIC_EXC 0

global VEC_DEBUG_HANDLER
VEC_DEBUG_HANDLER:
    GENERIC_EXC 1

global VEC_NMI_HANDLER
VEC_NMI_HANDLER:
    GENERIC_EXC 2

global VEC_BREAKPOVEC_HANDLER
VEC_BREAKPOVEC_HANDLER:
    GENERIC_EXC 3

global VEC_OVERFLOW_HANDLER
VEC_OVERFLOW_HANDLER:
    GENERIC_EXC 4

global VEC_OOB_HANDLER
VEC_OOB_HANDLER:
    GENERIC_EXC 5

global VEC_BAD_OP_HANDLER
VEC_BAD_OP_HANDLER:
    GENERIC_EXC 6

global VEC_DEV_NO_AVAILABLE
VEC_DEV_NOT_AVAILABLE:
    GENERIC_EXC 7

global VEC_DOUBLE_FAULT
VEC_DOUBLE_FAULT:
    GENERIC_EXC 8

global VEC_INVALID_TSS
VEC_INVALID_TSS:
    GENERIC_EXC 10

global VEC_SEG_NOT_PRESENT
VEC_SEG_NOT_PRESENT:
    GENERIC_EXC 11

global VEC_STACK_SEGFAULT
VEC_STACK_SEGFAULT:
    GENERIC_EXC 12

global VEC_PROT_FAULT
VEC_PROT_FAULT:
    GENERIC_EXC 13

global VEC_PAGE_FAULT
VEC_PAGE_FAULT:
    GENERIC_EXC 14

global VEC_FLOAT_FAULT
VEC_FLOAT_FAULT:
    GENERIC_EXC 16

global VEC_ALIGNMENT_CHECK
VEC_FLOAT_FAULT:
    GENERIC_EXC 17

global VEC_MACHINE_CHECK
VEC_MACHINE_CHECK:
    GENERIC_EXC 18

global VEC_SIMD_FAULT
VEC_SIMD_FAULT:
    GENERIC_EXC 19

global VEC_VIRT_FAULT
VEC_VIRT_FAULT:
    GENERIC_EXC 20

global VEC_CONTROL_PROT
VEC_CONTROL_PROT:
    VEC_CONTROL_PROT 21