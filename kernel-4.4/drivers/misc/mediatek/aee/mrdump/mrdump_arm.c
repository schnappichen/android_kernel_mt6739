
#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

void mrdump_save_current_backtrace(struct pt_regs *regs)
{
	asm volatile ("stmia %1, {r0 - r15}\n\t"
			"mrs %0, cpsr\n":"=r" (regs->uregs[16]) : "r"(regs) : "memory");
}

void mrdump_save_control_register(void *creg)
{
	struct arm32_ctrl_regs *cregs = (struct arm32_ctrl_regs *)creg;

	asm volatile ("mrc p15, 0, %0, c1, c0, 0\n\t"
		      "mrc p15, 0, %1, c2, c0, 2\n\t"
		      "mrc p15, 0, %2, c2, c0, 0\n\t"
		      "mrc p15, 0, %3, c2, c0, 1\n\t"
		      : "=r"(cregs->sctlr), "=r"(cregs->ttbcr),
		      "=r"(cregs->ttbr0), "=r"(cregs->ttbr1) : : "memory");
}
