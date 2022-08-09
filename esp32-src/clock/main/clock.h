#ifndef CLOCK_CLOCK_H
#define CLOCK_CLOCK_H

time_t clock_get_time(void);
void clock_add(uint64_t midpoint);
bool clock_is_synched(void);
time_t clock_time_since_last_update(void);

#endif //CLOCK_CLOCK_H
