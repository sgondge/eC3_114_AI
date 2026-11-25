#ifndef __STOVE_STATS_H
#define __STOVE_STATS_H

/* ---- HT32F54253 includes (Holtek Std Driver) ---- */
#include "ht32.h"
#include "ht32_board.h"


void pg_avg_reset(void);
void pg_accept_sample(float ag, float vg);  // feeds one (Ag, Vg) pair

// Optional: print current averages immediately
//void pg_avg_dump(void);

#endif
