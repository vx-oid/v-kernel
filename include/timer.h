#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned int systicks;
extern volatile unsigned int timer_delta;
extern volatile unsigned long long next_tick_alarm;

void clear_timer_interrupt(void);
void timer_init(unsigned int dummy_frequency_divider);
void timer_init_periodic(unsigned int delta_ticks);
unsigned long long read_timg0_counter(void);
void read_and_print_timg_status(void);
void read_and_print_timg_regs(void);

#endif
