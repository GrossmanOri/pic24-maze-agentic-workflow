/*******************************************************************************
 * ticktimer.h - Free-running millisecond time base (Timer1).
 *
 * Timer1 fires every TICK_MS and the ISR only advances a counter (no drawing -
 * the OLED/SPI bus is touched solely from the main context, so there is no
 * ISR/main bus contention). All game timing derives from tick_ms().
 ******************************************************************************/
#ifndef TICKTIMER_H
#define TICKTIMER_H

#include <stdint.h>

void     tick_init(void);   /* start Timer1 at TICK_MS period */
uint32_t tick_ms(void);     /* milliseconds since tick_init()  */

#endif /* TICKTIMER_H */
