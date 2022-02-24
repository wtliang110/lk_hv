/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Interception tests - for s390x CPU instruction that cause a VM exit
 *
 * Copyright (c) 2017 Red Hat Inc
 *
 * Authors:
 *  Thomas Huth <thuth@redhat.com>
 */
#include <libcflat.h>
#include <sclp.h>
#include <asm/asm-offsets.h>
#include <asm/interrupt.h>
#include <asm/page.h>
#include <asm/facility.h>
#include <asm/time.h>

static uint8_t pagebuf[PAGE_SIZE * 2] __attribute__((aligned(PAGE_SIZE * 2)));

static unsigned long nr_iterations;
static unsigned long time_to_run;

/* Test the STORE PREFIX instruction */
static void test_stpx(void)
{
	uint32_t old_prefix = -1U, tst_prefix = -1U;
	uint32_t new_prefix = (uint32_t)(intptr_t)pagebuf;

	/* Can we successfully change the prefix? */
	old_prefix = get_prefix();
	set_prefix(new_prefix);
	tst_prefix = get_prefix();
	set_prefix(old_prefix);
	report(old_prefix == 0 && tst_prefix == new_prefix, "store prefix");

	expect_pgm_int();
	low_prot_enable();
	asm volatile(" stpx 0(%0) " : : "r"(8));
	low_prot_disable();
	check_pgm_int_code(PGM_INT_CODE_PROTECTION);

	expect_pgm_int();
	asm volatile(" stpx 0(%0) " : : "r"(1));
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	asm volatile(" stpx 0(%0) " : : "r"(-8L));
	check_pgm_int_code(PGM_INT_CODE_ADDRESSING);
}

/* Test the SET PREFIX instruction */
static void test_spx(void)
{
	uint32_t new_prefix = (uint32_t)(intptr_t)pagebuf;
	uint32_t old_prefix;

	memset(pagebuf, 0, PAGE_SIZE * 2);

	/*
	 * Temporarily change the prefix page to our buffer, and store
	 * some facility bits there ... at least some of them should be
	 * set in our buffer afterwards.
	 */
	old_prefix = get_prefix();
	set_prefix(new_prefix);
	stfl();
	set_prefix(old_prefix);
	report(pagebuf[GEN_LC_STFL] != 0, "stfl to new prefix");

	expect_pgm_int();
	asm volatile(" spx 0(%0) " : : "r"(1));
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	asm volatile(" spx 0(%0) " : : "r"(-8L));
	check_pgm_int_code(PGM_INT_CODE_ADDRESSING);
}

/* Test the STORE CPU ADDRESS instruction */
static void test_stap(void)
{
	uint16_t cpuid = 0xffff;

	asm volatile ("stap %0\n" : "+Q"(cpuid));
	report(cpuid != 0xffff, "get cpu address");

	expect_pgm_int();
	low_prot_enable();
	asm volatile ("stap 0(%0)\n" : : "r"(8));
	low_prot_disable();
	check_pgm_int_code(PGM_INT_CODE_PROTECTION);

	expect_pgm_int();
	asm volatile ("stap 0(%0)\n" : : "r"(1));
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	asm volatile ("stap 0(%0)\n" : : "r"(-8L));
	check_pgm_int_code(PGM_INT_CODE_ADDRESSING);
}

/* Test the STORE CPU ID instruction */
static void test_stidp(void)
{
	struct cpuid id = {};

	asm volatile ("stidp %0\n" : "+Q"(id));
	report(id.type, "type set");
	report(!id.version || id.version == 0xff, "version valid");
	report(!id.reserved, "reserved bits not set");

	expect_pgm_int();
	low_prot_enable();
	asm volatile ("stidp 0(%0)\n" : : "r"(8));
	low_prot_disable();
	check_pgm_int_code(PGM_INT_CODE_PROTECTION);

	expect_pgm_int();
	asm volatile ("stidp 0(%0)\n" : : "r"(1));
	check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

	expect_pgm_int();
	asm volatile ("stidp 0(%0)\n" : : "r"(-8L));
	check_pgm_int_code(PGM_INT_CODE_ADDRESSING);
}

/* Test the TEST BLOCK instruction */
static void test_testblock(void)
{
	int cc;

	memset(pagebuf, 0xaa, PAGE_SIZE);

	asm volatile (
		" lghi	%%r0,0\n"
		" .insn	rre,0xb22c0000,0,%1\n"
		" ipm	%0\n"
		" srl	%0,28\n"
		: "=d" (cc)
		: "a"(pagebuf + 0x123)
		: "memory", "0", "cc");
	report(cc == 0 && pagebuf[0] == 0 && pagebuf[PAGE_SIZE - 1] == 0,
	       "page cleared");

	expect_pgm_int();
	low_prot_enable();
	asm volatile (" .insn	rre,0xb22c0000,0,%0\n" : : "r"(4096));
	low_prot_disable();
	check_pgm_int_code(PGM_INT_CODE_PROTECTION);

	expect_pgm_int();
	asm volatile (" .insn	rre,0xb22c0000,0,%0\n" : : "r"(-4096L));
	check_pgm_int_code(PGM_INT_CODE_ADDRESSING);
}

static void test_diag318(void)
{
	expect_pgm_int();
	enter_pstate();
	asm volatile("diag %0,0,0x318\n" : : "d" (0x42));
	check_pgm_int_code(PGM_INT_CODE_PRIVILEGED_OPERATION);

	if (!sclp_facilities.has_diag318)
		expect_pgm_int();

	asm volatile("diag %0,0,0x318\n" : : "d" (0x42));

	if (!sclp_facilities.has_diag318)
		check_pgm_int_code(PGM_INT_CODE_SPECIFICATION);

}

struct {
	const char *name;
	void (*func)(void);
	bool run_it;
} tests[] = {
	{ "stpx", test_stpx, false },
	{ "spx", test_spx, false },
	{ "stap", test_stap, false },
	{ "stidp", test_stidp, false },
	{ "testblock", test_testblock, false },
	{ "diag318", test_diag318, false },
	{ NULL, NULL, false }
};

static void parse_intercept_test_args(int argc, char **argv)
{
	int i, ti;
	bool run_all = true;

	for (i = 1; i < argc; i++) {
		if (!strcmp("-i", argv[i])) {
			i++;
			if (i >= argc)
				report_abort("-i needs a parameter");
			nr_iterations = atol(argv[i]);
		} else if (!strcmp("-t", argv[i])) {
			i++;
			if (i >= argc)
				report_abort("-t needs a parameter");
			time_to_run = atol(argv[i]);
		} else for (ti = 0; tests[ti].name != NULL; ti++) {
			if (!strcmp(tests[ti].name, argv[i])) {
				run_all = false;
				tests[ti].run_it = true;
				break;
			} else if (tests[ti + 1].name == NULL) {
				report_abort("Unsupported parameter '%s'",
					     argv[i]);
			}
		}
	}

	if (run_all) {
		for (ti = 0; tests[ti].name != NULL; ti++)
			tests[ti].run_it = true;
	}
}

int main(int argc, char **argv)
{
	uint64_t startclk;
	int ti;

	parse_intercept_test_args(argc, argv);

	if (nr_iterations == 0 && time_to_run == 0)
		nr_iterations = 1;

	report_prefix_push("intercept");

	startclk = get_clock_ms();
	for (;;) {
		for (ti = 0; tests[ti].name != NULL; ti++) {
			report_prefix_push(tests[ti].name);
			if (tests[ti].run_it)
				tests[ti].func();
			report_prefix_pop();
		}
		if (nr_iterations) {
			nr_iterations -= 1;
			if (nr_iterations == 0)
				break;
		}
		if (time_to_run) {
			if (get_clock_ms() - startclk > time_to_run)
				break;
		}
	}

	report_prefix_pop();

	return report_summary();
}
