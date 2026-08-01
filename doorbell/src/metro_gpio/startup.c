/* ---------------------------------------------------------------------------
 *  startup.c — everything that has to happen before main() on a Cortex-M4.
 *
 *  With no framework there is no crt0 and no vendor startup file, so this is
 *  the whole of it: a vector table at the very start of the image, and a reset
 *  handler that puts C's assumptions in place (initialised globals hold their
 *  initialisers, uninitialised globals are zero) and then calls main.
 *
 *  Anything the linker script exports is declared here as `extern uint32_t`
 *  and used by ADDRESS, never by value.  `_sdata` is not a variable holding a
 *  pointer; the linker places the SYMBOL at the address, so `&_sdata` is the
 *  address and reading `_sdata` would read whatever bytes happen to live
 *  there.  Getting this wrong is a classic and it fails at runtime, not at
 *  compile time.
 * ------------------------------------------------------------------------- */

#include <stdint.h>

extern uint32_t _sidata;   /* .data initialisers, in flash                */
extern uint32_t _sdata;    /* .data start, in RAM                         */
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;   /* top of RAM; the initial stack pointer       */

int  main(void);
void Reset_Handler(void);
void Default_Handler(void);

/* ---------------------------------------------------------------------------
 *  The vector table.
 *
 *  The linker script puts .vectors first in flash, so this lands at 0x4000 —
 *  the start of the application image.  Entry 0 is the initial stack pointer
 *  and entry 1 is the reset handler; the core loads both in hardware before
 *  the first instruction ever runs.
 *
 *  Only the 16 architectural (negative-numbered) exceptions are listed.  This
 *  program enables no interrupts at all, so the ~140 peripheral IRQ slots that
 *  would follow are simply absent — nothing can vector into them.  Add an
 *  interrupt source later and the table must grow to cover it.
 *
 *  All faults land in Default_Handler, which spins.  That is on purpose: a
 *  spin is findable with a debugger, whereas a fault handler that returns
 *  turns a hard fault into an infinite fault loop that looks like a hang.
 * ------------------------------------------------------------------------- */
__attribute__((section(".vectors"), used))
void (* const vector_table[])(void) =
{
    (void (*)(void))(&_estack),   /*  0  initial stack pointer  */
    Reset_Handler,                /*  1  reset                  */
    Default_Handler,              /*  2  NMI                    */
    Default_Handler,              /*  3  hard fault             */
    Default_Handler,              /*  4  memory management      */
    Default_Handler,              /*  5  bus fault              */
    Default_Handler,              /*  6  usage fault            */
    0, 0, 0, 0,                   /*  7-10  reserved            */
    Default_Handler,              /* 11  SVCall                 */
    Default_Handler,              /* 12  debug monitor          */
    0,                            /* 13  reserved               */
    Default_Handler,              /* 14  PendSV                 */
    Default_Handler,              /* 15  SysTick                */
};

void Default_Handler(void)
{
    for (;;) { }
}

void Reset_Handler(void)
{
    uint32_t *src, *dst;

    /* .data: copy initialisers from flash into RAM.  Until this runs, every
     * initialised global holds garbage. */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* .bss: zero it.  C guarantees uninitialised statics read as 0 and this
     * loop is the only thing that makes that true. */
    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0u;
    }

    /* Point the vector table offset register at our table.
     *
     * VTOR lives at 0xE000ED08 in the System Control Block.  The Adafruit UF2
     * bootloader normally sets this before jumping to 0x4000, but it costs one
     * store to not depend on that — and if VTOR is left pointing at the
     * bootloader's table, every fault and interrupt would dispatch into the
     * bootloader's handlers instead of ours. */
    *(volatile uint32_t *)0xE000ED08u = (uint32_t)(uintptr_t)vector_table;

    /* Enable the FPU (CPACR, 0xE000ED88): full access to coprocessors 10 and
     * 11.  The SAMD51 is a Cortex-M4F and the compiler is free to emit VFP
     * instructions for anything float-shaped; executing one with the FPU still
     * disabled is a usage fault at a completely unrelated line.  This program
     * uses no floats, so this is pure insurance against a later edit. */
    *(volatile uint32_t *)0xE000ED88u |= (0xFu << 20);
    __asm volatile ("dsb");
    __asm volatile ("isb");

    (void)main();

    /* main() never returns.  If it somehow does, stop here rather than run
     * off the end of the vector table. */
    for (;;) { }
}
