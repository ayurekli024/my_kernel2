#ifndef TIMER_H
#define TIMER_H

void init_timer(unsigned int freq);
void sleep(unsigned int ms);
unsigned int get_uptime();

#endif