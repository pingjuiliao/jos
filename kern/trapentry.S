/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
// 0 - 4
TRAPHANDLER_NOEC(_trap_divide_error, T_DIVIDE);
TRAPHANDLER_NOEC(_trap_debug_exception, T_DEBUG);
TRAPHANDLER_NOEC(_trap_non_maskable_interrupt, T_NMI);
TRAPHANDLER_NOEC(_trap_breakpoint, T_BRKPT);
TRAPHANDLER_NOEC(_trap_overflow, T_OFLOW);

// 5 - 9
TRAPHANDLER_NOEC(_trap_bounds_check, T_BOUND);
TRAPHANDLER_NOEC(_trap_illegal_opcode, T_ILLOP);
TRAPHANDLER_NOEC(_trap_device_not_available, T_DEVICE);
TRAPHANDLER(_trap_double_fault, T_DBLFLT);
/* TRAPHANDLER_NOEC()  reserved */

// 10 - 14
TRAPHANDLER(_trap_invalid_task_switch_segment, T_TSS);
TRAPHANDLER(_trap_segment_not_present, T_SEGNP);
TRAPHANDLER(_trap_stack_exception, T_STACK);
TRAPHANDLER(_trap_general_protection_fault, T_GPFLT);
TRAPHANDLER(_trap_page_fault, T_PGFLT);


// 15 - 19
/* TRAPHANDLER_NOEC() reserved */
TRAPHANDLER(_trap_floating_point_error, T_FPERR);
TRAPHANDLER_NOEC(_trap_alignment_check, T_ALIGN);
TRAPHANDLER(_trap_machine_check, T_MCHK);
TRAPHANDLER_NOEC(_trap_simd_floating_point_error, T_SIMDERR);

// 48
TRAPHANDLER_NOEC(_trap_system_call, T_SYSCALL);


/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    push %es;
    push %ds;
    pushal;
    push $GD_KD;
    mov (%esp), %ds;
    mov (%esp), %es;
    pop %edi;
    pushl %esp ;
    call trap ;
