#ifndef __WRAPPERS_H
#define __WRAPPERS_H

extern void interrupt_0();
extern void interrupt_1();
extern void interrupt_2();
extern void interrupt_3();
extern void interrupt_4();
extern void interrupt_5();
extern void interrupt_6();
extern void interrupt_7();
extern void exception_8();
extern void interrupt_9();
extern void exception_a();
extern void exception_b();
extern void exception_c();
extern void exception_d();
extern void exception_e();
extern void interrupt_f();
extern void interrupt_10();
extern void interrupt_11();
extern void interrupt_12();
extern void interrupt_13();
extern void interrupt_14();
extern void interrupt_15();
extern void interrupt_16();
extern void interrupt_17();
extern void interrupt_18();
extern void interrupt_19();
extern void interrupt_1a();
extern void interrupt_1b();
extern void interrupt_1c();
extern void interrupt_1d();
extern void interrupt_1e();
extern void interrupt_1f();
extern void irq_0();
extern void irq_1();
extern void irq_2();
extern void irq_3();
extern void irq_4();
extern void irq_5();
extern void irq_6();
extern void irq_7();
extern void irq_8();
extern void irq_9();
extern void irq_a();
extern void irq_b();
extern void irq_c();
extern void irq_d();
extern void irq_e();
extern void irq_f();
//extern void syscall_wrapper();
extern void interrupt_30();

static void (*_wrappers[49]) () =
{
	interrupt_0,
	interrupt_1,
	interrupt_2,
	interrupt_3,
	interrupt_4,
	interrupt_5,
	interrupt_6,
	interrupt_7,
	exception_8,
	interrupt_9,
	exception_a,
	exception_b,
	exception_c,
	exception_d,
	exception_e,
	interrupt_f,
	interrupt_10,
	interrupt_11,
	interrupt_12,
	interrupt_13,
	interrupt_14,
	interrupt_15,
	interrupt_16,
	interrupt_17,
	interrupt_18,
	interrupt_19,
	interrupt_1a,
	interrupt_1b,
	interrupt_1c,
	interrupt_1d,
	interrupt_1e,
	interrupt_1f,
	irq_0,
	irq_1,
	irq_2,
	irq_3,
	irq_4,
	irq_5,
	irq_6,
	irq_7,
	irq_8,
	irq_9,
	irq_a,
	irq_b,
	irq_c,
	irq_d,
	irq_e,
	irq_f,
	//syscall_wrapper
	interrupt_30
};

#endif