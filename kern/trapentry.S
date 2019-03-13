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
TRAPHANDLER_NOEC(_trap_no9, 9);

// 10 - 14
TRAPHANDLER(_trap_invalid_task_switch_segment, T_TSS);
TRAPHANDLER(_trap_segment_not_present, T_SEGNP);
TRAPHANDLER(_trap_stack_exception, T_STACK);
TRAPHANDLER(_trap_general_protection_fault, T_GPFLT);
TRAPHANDLER(_trap_page_fault, T_PGFLT);


// 15 - 19
TRAPHANDLER_NOEC(_trap_no15, 15);
TRAPHANDLER(_trap_floating_point_error, T_FPERR);
TRAPHANDLER_NOEC(_trap_alignment_check, T_ALIGN);
TRAPHANDLER(_trap_machine_check, T_MCHK);
TRAPHANDLER_NOEC(_trap_simd_floating_point_error, T_SIMDERR);

TRAPHANDLER_NOEC(_trap_20, 20);
TRAPHANDLER_NOEC(_trap_21, 21);
TRAPHANDLER_NOEC(_trap_22, 22);
TRAPHANDLER_NOEC(_trap_23, 23);
TRAPHANDLER_NOEC(_trap_24, 24);
TRAPHANDLER_NOEC(_trap_25, 25);
TRAPHANDLER_NOEC(_trap_26, 26);
TRAPHANDLER_NOEC(_trap_27, 27);
TRAPHANDLER_NOEC(_trap_28, 28);
TRAPHANDLER_NOEC(_trap_29, 29);
TRAPHANDLER_NOEC(_trap_30, 30);
TRAPHANDLER_NOEC(_trap_31, 31);


// 32 - 47 : IRQs (hardware interrupt)
TRAPHANDLER_NOEC(_trap_irq_timer, IRQ_OFFSET + IRQ_TIMER);
TRAPHANDLER_NOEC(_trap_irq_kbd, IRQ_OFFSET + IRQ_KBD);
TRAPHANDLER_NOEC(_trap_irq_2, IRQ_OFFSET + 2);
TRAPHANDLER_NOEC(_trap_irq_3, IRQ_OFFSET + 3);
TRAPHANDLER_NOEC(_trap_irq_serial, IRQ_OFFSET + IRQ_SERIAL);

TRAPHANDLER_NOEC(_trap_irq_5, IRQ_OFFSET + 5);
TRAPHANDLER_NOEC(_trap_irq_6, IRQ_OFFSET + 6);
TRAPHANDLER_NOEC(_trap_irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS);
TRAPHANDLER_NOEC(_trap_irq_8, IRQ_OFFSET + 8);
TRAPHANDLER_NOEC(_trap_irq_9, IRQ_OFFSET + 9);

TRAPHANDLER_NOEC(_trap_irq_10, IRQ_OFFSET + 10);
TRAPHANDLER_NOEC(_trap_irq_11, IRQ_OFFSET + 11);
TRAPHANDLER_NOEC(_trap_irq_12, IRQ_OFFSET + 12);
TRAPHANDLER_NOEC(_trap_irq_13, IRQ_OFFSET + 13);
TRAPHANDLER_NOEC(_trap_irq_ide, IRQ_OFFSET + IRQ_IDE);
TRAPHANDLER_NOEC(_trap_irq_15, IRQ_OFFSET + 15);


// 48
TRAPHANDLER_NOEC(_trap_system_call, T_SYSCALL);
// 51
TRAPHANDLER_NOEC(_trap_irq_error, IRQ_OFFSET + IRQ_ERROR);

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
