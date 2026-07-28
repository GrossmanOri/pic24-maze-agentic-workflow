/*******************************************************************************
 * maze.h - Maze model, rendering and collision for the Maze game.
 *
 * The maze is a grid of cells (8x8, 10x10 or 12x12 depending on difficulty)
 * on the 96x96 OLED. Walls live on cell edges and are stored two ways:
 *   - logical wall arrays (vwall/hwall)           -> generation + solvability
 *   - a 96x96 1-bit occupancy bitmap (rasterized) -> fast pixel collision
 ******************************************************************************/
#ifndef MAZE_H
#define MAZE_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

/* Build the maze (walls + collision bitmap) from a deterministic seed, using a
 * cols x rows grid (clamped to MAZE_COLS/MAZE_ROWS). Bigger grid = harder. */
void maze_generate(uint32_t seed, int cols, int rows);

/* Draw the whole maze (walls + start/finish markers) to the OLED. */
void maze_render(void);

/* Redraw just the start and finish markers (call after erasing the ball). */
void maze_draw_markers(void);

/* Repaint wall pixels inside a rectangle (after a UI overlay erased them). */
void maze_redraw_region(int x0, int y0, int x1, int y1);

/* True if pixel (x,y) is a wall (or outside the panel). */
bool maze_is_wall_px(int x, int y);

/* True if a ball of radius r centered at (cx,cy) hits a wall or the bounds. */
bool maze_ball_collides(int cx, int cy, int r);

/* Center pixel of the start cell (where the ball begins). */
void maze_start_pixel(int *x, int *y);

/* True if a ball of the given radius centered at (cx,cy) touches the green
 * door drawn in the finish room. */
bool maze_at_finish(int cx, int cy, int radius);

/* Collectible dots: one per eligible cell, placed by maze_generate(). */
void maze_dots_render(void);
bool maze_dot_collect(int bx, int by, int radius);   /* true = ate a dot */
int  maze_dots_left(void);
int  maze_dots_total(void);
void maze_dots_redraw_region(int x0, int y0, int x1, int y1);

#endif /* MAZE_H */
