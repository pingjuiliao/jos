#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};

void _trap_divide_error();
void _trap_debug_exception();
void _trap_non_maskable_interrupt();
void _trap_breakpoint();
void _trap_overflow();

void _trap_bounds_check();
void _trap_illegal_opcode();
void _trap_device_not_available();
void _trap_double_fault();
/* void _trap_number9();*/

void _trap_invalid_task_switch_segment();
void _trap_segment_not_present();
void _trap_stack_exception();
void _trap_general_protection_fault();
void _trap_page_fault();

/* void _trap_number15() */
void _trap_floating_point_error();
void _trap_alignment_check();
void _trap_machine_check();
void _trap_simd_floating_point_error();
void _trap_system_call();


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.

    // SETGATE(gate, istrap, sel, off, dpl)
    // - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
    //
    SETGATE(idt[T_DIVIDE] , 1, GD_KT, _trap_divide_error, 0);
    SETGATE(idt[T_DEBUG]  , 1, GD_KT, _trap_debug_exception, 0);
    SETGATE(idt[T_NMI]    , 0, GD_KT, _trap_non_maskable_interrupt, 0);
    SETGATE(idt[T_BRKPT]  , 1, GD_KT, _trap_breakpoint, 3);
    SETGATE(idt[T_OFLOW]  , 1, GD_KT, _trap_overflow, 0);
    SETGATE(idt[T_BOUND]  , 1, GD_KT, _trap_bounds_check, 0);
    SETGATE(idt[T_ILLOP]  , 1, GD_KT, _trap_illegal_opcode, 0);
    SETGATE(idt[T_DEVICE] , 1, GD_KT, _trap_device_not_available, 0);
    SETGATE(idt[T_DBLFLT] , 1, GD_KT, _trap_double_fault, 0);
    SETGATE(idt[T_TSS]    , 1, GD_KT, _trap_invalid_task_switch_segment, 0);
    SETGATE(idt[T_SEGNP]  , 1, GD_KT, _trap_segment_not_present, 0);
    SETGATE(idt[T_STACK]  , 1, GD_KT, _trap_stack_exception, 0);
    SETGATE(idt[T_GPFLT]  , 1, GD_KT, _trap_general_protection_fault, 0);
    SETGATE(idt[T_PGFLT]  , 1, GD_KT, _trap_page_fault, 0);
    SETGATE(idt[T_FPERR]  , 1, GD_KT, _trap_floating_point_error, 0);
    SETGATE(idt[T_ALIGN]  , 1, GD_KT, _trap_alignment_check, 0);
    SETGATE(idt[T_MCHK]   , 1, GD_KT, _trap_machine_check, 0);
    SETGATE(idt[T_SIMDERR], 1, GD_KT, _trap_simd_floating_point_error, 0);
    SETGATE(idt[T_SYSCALL], 1, GD_KT, _trap_system_call, 3);
    // Per-CPU setup
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;
	ts.ts_iomb = sizeof(struct Taskstate);

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
#ifdef DEBUG
    cprintf("trap_dispatch: tf->tf_trapno == 0x%08x\n", tf->tf_trapno);
#endif
    switch(tf->tf_trapno) {

    case T_BRKPT: /* 3 */
        monitor(tf);
        return ;
    case T_PGFLT : /* 14 */
#ifdef DEBUG
        cprintf("trap_dispatch causing page fault!!!!\n");
#endif
        page_fault_handler(tf);
        return;
    case T_SYSCALL: /* 48 */
        tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax,
                    tf->tf_regs.reg_edx,
                    tf->tf_regs.reg_ecx,
                    tf->tf_regs.reg_ebx,
                    tf->tf_regs.reg_edi,
                    tf->tf_regs.reg_esi);

        return ;
    default:
        break ;
    }

    // Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
#ifdef DEBUG
	cprintf("\n########################\n");
    cprintf("trap: Entering kernel mode\n");
#endif
    // assert( curenv->env_pgdir[PDX(UENVS)] == kern_pgdir[PDX(UENVS)] );
    asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
    // assert( curenv->env_pgdir[PDX(UENVS)] == kern_pgdir[PDX(UENVS)] );
	trap_dispatch(tf);
    //    assert( curenv->env_pgdir[PDX(UENVS)] == kern_pgdir[PDX(UENVS)] );

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
    struct PageInfo *pp ;
#ifdef DEBUG
    cprintf("page_fault_handler: tf->tf_cs == 0x%08x\n", tf->tf_cs);
#endif
    if ( ( tf->tf_cs & 0x00000008 ) == 0 ) {
         panic("page_fault_handler: kernel-mode exception");
    }

    // We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

