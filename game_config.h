/*******************************************************************************
 * game_config.h - Shared constants and tuning parameters for the Maze game.
 *
 * Final project: "Ball in a Maze" simulator for the PIC24FJ256GA705 Curiosity
 * board. The board is tilted to roll a ball (read via the ADXL345
 * accelerometer) from START to FINISH of a 96x96 maze on the OLED, within a
 * time limit set by the chosen difficulty.
 *
 * All tuning values (timing, ball feel, maze geometry, colors) are collected
 * here so they can be changed in one place.
 ******************************************************************************/
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <stdint.h>

/* ---- Display ----------------------------------------------------------- */
#define SCREEN_W            96
#define SCREEN_H            96

/* ---- Maze grid --------------------------------------------------------- */
/* The grid size is chosen per difficulty at run time (see GRID_LVL*). Walls
 * sit on cell boundaries and are drawn 1 pixel wide. The arrays are sized to
 * the largest grid; MAZE_COLS/ROWS are the MAX. */
#define MAZE_COLS           12      /* max columns (array bound) */
#define MAZE_ROWS           12      /* max rows    (array bound) */

/* Per-difficulty grid: bigger grid = smaller cells = denser, harder maze.
 * Levels 1 and 2 divide 96 evenly; level 3 leaves its last row/column a bit
 * wider (xline() clamps the outer edge to the panel). */
#define GRID_LVL1           6       /* 16 px cells - easy   */
#define GRID_LVL2           8       /* 12 px cells - medium */
#define GRID_LVL3           10      /* ~9 px cells - hard   */

/* Layout (computed at run time in maze.c from the active grid): START is the
 * BOTTOM-center cell, FINISH is a 2x2 room in the CENTER. */

/* ---- Ball -------------------------------------------------------------- */
/* The radius comes from the difficulty (levels 1-2 use a bigger ball, the
 * dense level-3 maze needs the small one to fit its corridors). */
#define BALL_R_BIG          3
#define BALL_R_SMALL        2
#define BALL_MAX_STEP       5       /* max pixels moved per axis per frame   */

/* ---- Tilt / physics ----------------------------------------------------
 * The ADXL345 reads ~256 counts per g; a flat board reads ~0 on X and Y.
 * Ball motion uses integer fixed-point (no FPU on PIC24): position carries a
 * 1/SUBPIXEL fractional accumulator so gentle tilts still creep smoothly.
 *
 *   velocity[1/16 px per frame] = tilt_counts * speed_pct / TILT_VEL_DEN
 *
 * With TILT_VEL_DEN = 300 a full sideways g (256) at 100% gives about
 * 5 px/frame (before the dead-zone), just inside the BALL_MAX_STEP clamp.
 *
 * ACCEL_*_SIGN map the chip axes to screen axes (screen +x = right, +y =
 * down), matching how the Accel Click is mounted on this board. */
#define TILT_DEADZONE       12      /* ignore small tilts (counts)           */
#define SUBPIXEL            16       /* fractional position resolution        */
#define TILT_VEL_DEN        300      /* velocity scaling denominator          */
#define ACCEL_X_SIGN        (-1)     /* screen vx = ACCEL_X_SIGN * accel_x    */
#define ACCEL_Y_SIGN        (+1)     /* screen vy = ACCEL_Y_SIGN * accel_y    */

/* Per-difficulty speed factor expressed as a percentage (avoids floats):
 * level 1 -> 0.8 (80%), level 2 -> 1.0 (100%), level 3 -> 1.2 (120%). */
#define SPEED_PCT_LVL1      80
#define SPEED_PCT_LVL2      100
#define SPEED_PCT_LVL3      120

/* Allowed time per difficulty, in tenths of a second (40 / 30 / 20 s). */
#define TIME_TENTHS_LVL1    400
#define TIME_TENTHS_LVL2    300
#define TIME_TENTHS_LVL3    200

/* ---- Timing ------------------------------------------------------------ */
#define TICK_MS             10      /* Timer1 ISR period (ms)                */
#define HURRY_UP_TENTHS     50      /* last 5.0 s -> blink the screen        */
#define BLINK_TOGGLE_MS     500     /* invert every 500ms => 2 swaps/second  */
#define LONGPRESS_MS        2000    /* hold S1 2 s to abort to the menu      */

/* ---- High scores ------------------------------------------------------- */
#define NUM_HIGHSCORES      3       /* the "big three"                       */
#define NAME_MAXLEN         12      /* up to 12 characters per name          */

/* ---- Collectible dots ---------------------------------------------------
 * One dot per corridor cell (skipping the start cell, the finish room and
 * the timer corner). Purely for fun - the score stays the game time, as the
 * brief defines. */
#define DOT_KEEPOUT_X       34      /* no dots under the timer pad...        */
#define DOT_KEEPOUT_Y       20      /* ...(cell centers left of/above this)  */
#define COIN_KEEPOUT_X      74      /* ...nor under the coin counter (right) */

/* ---- Countdown --------------------------------------------------------- */
#define COUNTDOWN_STEP_MS   650     /* per digit of the 3-2-1 countdown      */

/* ---- Input feel --------------------------------------------------------- */
#define POT_HYST            8       /* ADC counts into a band before the pot
                                     * index switches (kills letter flicker) */
#define SAVE_HOLD_MS        400     /* hold S1+S2 this long to save the name */

/* ---- Colors (RGB565) --------------------------------------------------- */
#define COL_BG              0x0000  /* screen background                     */
#define COL_WALL            0xFFFF  /* maze walls                            */
#define COL_BALL            0xF800  /* the ball                              */
#define COL_FINISH          0x07E0  /* finish room (reads green = "go")      */
#define COL_TEXT            0xFFFF  /* regular text                          */
#define COL_DIM             0x07FF  /* secondary text (timer, hints)         */
#define COL_HILITE          0xFC00  /* menu selection / accents              */
#define COL_DOT             0xFFE0  /* collectible coins (reads gold)        */

/* Difficulty record. */
typedef struct {
    uint16_t time_tenths;   /* allowed time, tenths of a second */
    uint8_t  speed_pct;     /* ball speed factor, percent       */
    uint8_t  ball_r;        /* ball radius in pixels            */
} Difficulty;

#endif /* GAME_CONFIG_H */
