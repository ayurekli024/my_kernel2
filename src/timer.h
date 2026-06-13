#ifndef TIMER_H
#define TIMER_H

extern volatile unsigned int timer_ticks;
void init_timer(unsigned int frequency);
void timer_handler_main(void);

#endif