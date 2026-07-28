/*******************************************************************************
 * input.c - Potentiometer + S1/S2 buttons for the Maze game.
 *
 * Pin map (PIC24FJ256GA705 Curiosity board):
 *   S1  = RA11 (digital input, active-low, weak pull-up)
 *   S2  = RA12 (digital input, active-low, weak pull-up)
 *   POT = RB12 = AN8 (analog input; the pin is RB12 but its ADC channel is AN8)
 ******************************************************************************/
#include <xc.h>
#include "input.h"
#include "game_config.h"

/* ---- Debounce / edge state --------------------------------------------- */
#define DEBOUNCE_MS   20

typedef struct {
    bool     stable;        /* debounced "down" state          */
    bool     raw_last;      /* last raw sample                 */
    uint32_t raw_since;     /* time the raw sample last changed */
    bool     edge;          /* pending press edge (one-shot)   */
} Button;

static Button s1, s2;
static uint32_t s1_down_since;     /* when S1 last transitioned to down */
static bool     s1_long_fired;     /* long-press already reported       */
static bool     s1_long_event;     /* pending long-press one-shot       */

/* ---- Button pin reads (active-low) ------------------------------------- */
static bool read_s1_raw(void) { return PORTAbits.RA11 == 0; }
static bool read_s2_raw(void) { return PORTAbits.RA12 == 0; }

static void debounce(Button *b, bool raw, uint32_t now)
{
    if (raw != b->raw_last) {
        b->raw_last = raw;
        b->raw_since = now;
    } else if ((now - b->raw_since) >= DEBOUNCE_MS && b->stable != raw) {
        b->stable = raw;
        if (raw) b->edge = true;     /* count a fresh press */
    }
}

void input_init(void)
{
    /* --- Buttons: RA11 / RA12 as digital inputs with weak pull-ups --- */
    TRISAbits.TRISA11 = 1;
    TRISAbits.TRISA12 = 1;
    /* RA11/RA12 have no ANSEL bit on this part, so they are digital. */
    IOCPUAbits.IOCPUA11 = 1; /* enable weak pull-up on RA11 */
    IOCPUAbits.IOCPUA12 = 1; /* enable weak pull-up on RA12 */

    /* --- Potentiometer on pin RB12 (ADC channel AN8) --- */
    ANSA = 0x000F;   /* unused RA0-RA3 left analog (their reset default) */
    ANSB = 0x100C;   /* RB2,RB3,RB12 analog; leave RB9 DIGITAL (it is I2C1 SDA) */
    TRISBbits.TRISB12 = 1;

    /* --- 10-bit ADC, manual sampling, channel AN8 --- */
    AD1CON1 = 0x0000;
    AD1CON2 = 0x0000;
    AD1CON3 = 0x0000;
    AD1CON3bits.ADCS = 0xFF;     /* slow A/D clock (safe)            */
    AD1CON3bits.SAMC = 0x10;     /* auto-sample time                */
    AD1CHS  = 0x0000;
    AD1CHSbits.CH0SA = 8;        /* select AN8 == pin RB12 (potentiometer) */
    AD1CON1bits.ADON = 1;        /* turn the ADC on                  */

    s1.raw_last = s2.raw_last = false;
    s1.stable = s2.stable = false;
}

void input_poll(uint32_t now_ms)
{
    debounce(&s1, read_s1_raw(), now_ms);
    debounce(&s2, read_s2_raw(), now_ms);

    /* Track S1 long-press. */
    static bool s1_prev_down = false;
    if (s1.stable && !s1_prev_down) {
        s1_down_since = now_ms;
        s1_long_fired = false;
    }
    if (s1.stable && !s1_long_fired &&
        (now_ms - s1_down_since) >= LONGPRESS_MS) {
        s1_long_fired = true;
        s1_long_event = true;
    }
    s1_prev_down = s1.stable;
}

bool input_both_down(void) { return s1.stable && s2.stable; }

bool input_s1_pressed(void) { bool e = s1.edge; s1.edge = false; return e; }
bool input_s2_pressed(void) { bool e = s2.edge; s2.edge = false; return e; }

bool input_s1_long_press(void) { bool e = s1_long_event; s1_long_event = false; return e; }

static uint16_t input_pot_raw(void)
{
    uint16_t guard = 0;
    AD1CON1bits.SAMP = 1;                 /* start sampling      */
    { volatile int i; for (i = 0; i < 200; i++) ; }  /* sample settle */
    AD1CON1bits.SAMP = 0;                 /* start conversion    */
    /* Bounded wait: never hang the whole game on a stuck ADC. */
    while (!AD1CON1bits.DONE && ++guard) ;
    return (uint16_t)ADC1BUF0;            /* 0..1023             */
}

/* Maps the pot to 0..num_steps-1 with hysteresis: when the raw value sits
 * right on a step boundary the ADC noise would flicker between the two
 * neighbours, so a new step is only accepted once the pot is clearly inside
 * its band. */
uint8_t input_pot_index(uint8_t num_steps)
{
    static uint8_t last_steps = 0, last_idx = 0;
    uint16_t v = input_pot_raw();         /* 0..1023 */
    uint8_t idx = (uint8_t)((uint32_t)v * num_steps / 1024u);
    if (idx >= num_steps) idx = (uint8_t)(num_steps - 1);

    if (num_steps != last_steps) {        /* new caller: take it as-is */
        last_steps = num_steps;
        last_idx = idx;
    } else if (idx != last_idx) {
        uint16_t lo = (uint16_t)((uint32_t)idx * 1024u / num_steps);
        uint16_t hi = (uint16_t)((uint32_t)(idx + 1) * 1024u / num_steps);
        if (v >= lo + POT_HYST && v + POT_HYST < hi)
            last_idx = idx;
    }
    return last_idx;
}
