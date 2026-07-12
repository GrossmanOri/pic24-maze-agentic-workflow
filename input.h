/*******************************************************************************
 * input.h - Potentiometer (ADC AN12/RB12) and push buttons S1 (RA11), S2
 * (RA12) for the Maze game.
 *
 * Buttons are active-low (pressed == 0) with internal pull-ups enabled.
 * input_poll() must be called regularly with the current millisecond time
 * (from the game tick) so edges, debouncing and the S1 long-press are tracked.
 ******************************************************************************/
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

void input_init(void);

/* Sample buttons and update edge/long-press state. now_ms = running ms time. */
void input_poll(uint32_t now_ms);

/* Level state (debounced). */
bool input_s1_down(void);
bool input_s2_down(void);
bool input_both_down(void);          /* S1 AND S2 held together */

/* One-shot press edges (true once per press; cleared on read). */
bool input_s1_pressed(void);
bool input_s2_pressed(void);

/* Fires once when S1 has been held continuously for LONGPRESS_MS. */
bool input_s1_long_press(void);

/* Potentiometer. */
uint16_t input_pot_raw(void);                 /* 0..1023                        */
uint8_t  input_pot_index(uint8_t num_steps);  /* maps pot to 0..num_steps-1     */

#endif /* INPUT_H */
