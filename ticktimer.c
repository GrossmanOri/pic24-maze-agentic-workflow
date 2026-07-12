/*******************************************************************************
 * ticktimer.c - Timer1 millisecond time base.
 *
 * FCY = 4 MHz, prescaler 1:8 -> 2 us/tick. PR1 = (TICK_MS ms / 2 us) - 1.
 * For TICK_MS = 10: 10000 us / 2 us = 5000 ticks -> PR1 = 4999 (exactly 10 ms).
 ******************************************************************************/
#include <xc.h>
#include "ticktimer.h"
#include "game_config.h"

#define T1_PRESCALE_8   0x0010
#define T1_ON           0x8000
#define PR1_TICK        (5000 - 1)   /* 10 ms at FCY=4MHz, 1:8 */

static volatile uint32_t g_ms = 0;

void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void)
{
    g_ms += TICK_MS;
    IFS0bits.T1IF = 0;
}

void tick_init(void)
{
    g_ms = 0;
    T1CONbits.TON = 0;
    TMR1 = 0;
    PR1  = PR1_TICK;
    T1CON = T1_ON | T1_PRESCALE_8;   /* internal clock, 1:8, timer on */

    IPC0bits.T1IP = 0x01;
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
}

uint32_t tick_ms(void)
{
    uint32_t m;
    IEC0bits.T1IE = 0;       /* 32-bit read is non-atomic on PIC24 */
    m = g_ms;
    IEC0bits.T1IE = 1;
    return m;
}
