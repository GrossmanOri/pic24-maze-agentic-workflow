/*******************************************************************************
 * ball.h - The rolling ball: physics, wall collision and draw/erase.
 *
 * The ball is a filled circle. Tilt (from the accelerometer) accelerates it;
 * walls and window bounds stop it. Motion is per-axis so axis-aligned maze
 * corridors behave naturally. Rendering erases the ball at its old spot in
 * the background color, then draws it at the new spot.
 ******************************************************************************/
#ifndef BALL_H
#define BALL_H

#include <stdint.h>

/* Place the ball at pixel (x,y) and clear its motion state. */
void ball_reset(int x, int y);

/* Advance one frame: tilt counts tx,ty and the difficulty speed factor (%).
 * Applies dead-zone, fixed-point velocity and per-axis wall collision. */
void ball_update(int tx, int ty, uint8_t speed_pct);

/* Erase the ball at its previous position and draw it at the current one
 * (only repaints if it actually moved). */
void ball_render(void);

/* Draw the ball sprite (filled circle + glint) at an arbitrary position. */
void ball_draw_at(int x, int y);

/* Current ball center. */
int ball_x(void);
int ball_y(void);

#endif /* BALL_H */
