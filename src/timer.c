#include "hal.h"
#include "timer.h"

volatile unsigned int systicks = 0;
volatile unsigned int timer_delta = 0;
volatile unsigned long long next_tick_alarm = 0;

unsigned long long read_timg0_counter(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *t0update = base + (0x000C / 4);
    volatile unsigned int *t0lo_read = base + (0x0004 / 4);
    volatile unsigned int *t0hi_read = base + (0x0008 / 4);

    *t0update = 1u;
    while ((*t0update & 1u) != 0);

    unsigned long long lo = (unsigned long long)(*t0lo_read);
    unsigned long long hi = (unsigned long long)(*t0hi_read);
    return (hi << 32) | lo;
}

void read_and_print_timg_status(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *int_ena = base + (0x0070/4);
    volatile unsigned int *int_raw = base + (0x0074/4);
    volatile unsigned int *int_st  = base + (0x0078/4);

    v_print(" TIMG: int_raw="); v_print_hex(*int_raw);
    v_print(" int_st="); v_print_hex(*int_st);
    v_print(" int_ena="); v_print_hex(*int_ena);
}

void read_and_print_timg_regs(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *t0cfg = base + (0x0000/4);
    volatile unsigned int *t0lo = base + (0x0010/4);
    volatile unsigned int *t0hi = base + (0x0014/4);
    volatile unsigned int *t0lo_read = base + (0x0004/4);
    volatile unsigned int *t0hi_read = base + (0x0008/4);

    v_print(" T0CFG="); v_print_hex(*t0cfg);
    v_print(" T0ALARMLO="); v_print_hex(*t0lo);
    v_print(" T0ALARMHI="); v_print_hex(*t0hi);
    volatile unsigned int *t0update = base + (0x000C/4);
    *t0update = 1u;
    while ((*t0update & 1u) != 0);
    v_print(" T0LO="); v_print_hex(*t0lo_read);
    v_print(" T0HI="); v_print_hex(*t0hi_read);
}

void clear_timer_interrupt(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *timg_int_clr = base + (0x007C/4);
    volatile unsigned int *t0update = base + (0x000C/4);
    volatile unsigned int *t0lo_read = base + (0x0004/4);
    volatile unsigned int *t0hi_read = base + (0x0008/4);
    volatile unsigned int *t0lo = base + (0x0010/4);
    volatile unsigned int *t0hi = base + (0x0014/4);

    *timg_int_clr = 1u;

    *t0update = 1u;
    while ((*t0update & 1u) != 0);

    unsigned int lo = *t0lo_read;
    unsigned int hi = *t0hi_read;
    unsigned long long cur = ((unsigned long long)hi << 32) | (unsigned long long)lo;
    unsigned long long alarm = cur + (unsigned long long)timer_delta;

    *t0lo = (unsigned int)(alarm & 0xFFFFFFFFu);
    *t0hi = (unsigned int)(alarm >> 32);
}

void timer_init(unsigned int dummy_frequency_divider) {
    (void)dummy_frequency_divider;
    volatile unsigned int *timg_regclk = (volatile unsigned int *)(0x6001F000 + 0x00FC);
    *timg_regclk |= (1u << 30);

    volatile unsigned int *timg_int_ena = (volatile unsigned int *)(0x6001F000 + 0x0070);
    *timg_int_ena |= 1u;

    asm volatile ("csrrs zero, mstatus, %0" :: "r" (1u << 3));
}

void timer_init_periodic(unsigned int delta_ticks) {
    timer_delta = delta_ticks;
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *t0cfg = base + (0x0000/4);
    volatile unsigned int *t0lo = base + (0x0010/4);
    volatile unsigned int *t0hi = base + (0x0014/4);
    volatile unsigned int *t0update = base + (0x000C/4);
    volatile unsigned int *t0lo_read = base + (0x0004/4);
    volatile unsigned int *t0hi_read = base + (0x0008/4);
    volatile unsigned int *timg_regclk = base + (0x00FC/4);
    volatile unsigned int *timg_int_ena = base + (0x0070/4);

    *timg_regclk |= (1u << 30);

    *t0cfg &= ~(1u << 31);

    *t0cfg &= ~(((1u << 16) - 1u) << 13);
    *t0cfg |= (80u << 13);
    *t0cfg |= (1u << 12);

    *t0update = 1u;
    while ((*t0update & 1u) != 0);

    unsigned int lo = *t0lo_read;
    unsigned int hi = *t0hi_read;
    unsigned long long cur = ((unsigned long long)hi << 32) | (unsigned long long)lo;
    unsigned long long alarm = cur + (unsigned long long)delta_ticks;

    *t0lo = (unsigned int)(alarm & 0xFFFFFFFFu);
    *t0hi = (unsigned int)(alarm >> 32);

    next_tick_alarm = alarm;

    *timg_int_ena |= 1u;

    unsigned int cfg = *t0cfg;
    cfg |= (1u << 30);
    cfg |= (1u << 29);
    cfg |= (1u << 11);
    cfg |= (1u << 31);
    *t0cfg = cfg;
}
