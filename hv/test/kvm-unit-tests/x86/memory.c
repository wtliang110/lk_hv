/*
 * Test for x86 cache and memory instructions
 *
 * Copyright (c) 2015 Red Hat Inc
 *
 * Authors:
 *  Eduardo Habkost <ehabkost@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 */

#include "libcflat.h"
#include "desc.h"
#include "processor.h"

static long target;
static volatile int ud;
static volatile int isize;

static void handle_ud(struct ex_regs *regs)
{
	ud = 1;
	regs->rip += isize;
}

int main(int ac, char **av)
{
	int expected;

	handle_exception(UD_VECTOR, handle_ud);

	/* 3-byte instructions: */
	isize = 3;

	expected = !this_cpu_has(X86_FEATURE_CLFLUSH); /* CLFLUSH */
	ud = 0;
	asm volatile("clflush (%0)" : : "b" (&target));
	report(ud == expected, "clflush (%s)", expected ? "ABSENT" : "present");

	expected = !this_cpu_has(X86_FEATURE_XMM); /* SSE */
	ud = 0;
	asm volatile("sfence");
	report(ud == expected, "sfence (%s)", expected ? "ABSENT" : "present");

	expected = !this_cpu_has(X86_FEATURE_XMM2); /* SSE2 */
	ud = 0;
	asm volatile("lfence");
	report(ud == expected, "lfence (%s)", expected ? "ABSENT" : "present");

	ud = 0;
	asm volatile("mfence");
	report(ud == expected, "mfence (%s)", expected ? "ABSENT" : "present");

	/* 4-byte instructions: */
	isize = 4;

	expected = !this_cpu_has(X86_FEATURE_CLFLUSHOPT); /* CLFLUSHOPT */
	ud = 0;
	/* clflushopt (%rbx): */
	asm volatile(".byte 0x66, 0x0f, 0xae, 0x3b" : : "b" (&target));
	report(ud == expected, "clflushopt (%s)",
	       expected ? "ABSENT" : "present");

	expected = !this_cpu_has(X86_FEATURE_CLWB); /* CLWB */
	ud = 0;
	/* clwb (%rbx): */
	asm volatile(".byte 0x66, 0x0f, 0xae, 0x33" : : "b" (&target));
	report(ud == expected, "clwb (%s)", expected ? "ABSENT" : "present");

	ud = 0;
	/* clwb requires a memory operand, the following is NOT a valid
	 * CLWB instruction (modrm == 0xF0).
	 */
	asm volatile(".byte 0x66, 0x0f, 0xae, 0xf0");
	report(ud, "invalid clwb");

	expected = !this_cpu_has(X86_FEATURE_PCOMMIT); /* PCOMMIT */
	ud = 0;
	/* pcommit: */
	asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8");
	report(ud == expected, "pcommit (%s)", expected ? "ABSENT" : "present");

	return report_summary();
}
