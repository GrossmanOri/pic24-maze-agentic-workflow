/*******************************************************************************
 * game_config.h - Shared constants and tuning parameters for the Maze game.
 *
 * Final project: "Ball in a Maze" simulator for the PIC24FJ256GA705 Curiosity
 * board. The board is tilted to roll a ball (read via the ADXL345
 * accelerometer) from START to FINISH of a 96x96 maze on the OLED, within a
 * time limit set by the chosen difficulty.
 *
 * All values a grader / player might want to tweak (timing, ball feel, maze
 * geometry, colors) live here so the rest of the code stays declarative.
 ******************************************************************************/
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <stdint.h>

/* ---- Display ----------------------------------------------------------- */
#define SCREEN_W            96
#define SCREEN_H            96

/* ---- Maze grid --------------------------------------------------------- */
/* 8x8 logical cells over the 96x96 panel. Walls sit on cell boundaries and
 * are drawn 1 pixel wide. Cell pitch = 12 px. */
#define MAZE_COLS           12
#define MAZE_ROWS           12
#define CELL_PX             8       /* nominal cell pitch in pixels          */

/* START cell (top-left) and FINISH cell (bottom-right). */
#define START_COL           0
#define START_ROW           0
#define FINISH_COL          (MAZE_COLS - 1)
#define FINISH_ROW          (MAZE_ROWS - 1)

/* ---- Ball -------------------------------------------------------------- */
#define BALL_RADIUS         2       /* filled circle radius in pixels        */
#define BALL_MAX_STEP       3       /* max pixels moved per axis per frame   */

/* ---- Tilt / physics ----------------------------------------------------
 * The ADXL345 reads ~256 counts per g; a flat board reads ~0 on X and Y.
 * Ball motion uses integer fixed-point (no FPU on PIC24): position carries a
 * 1/SUBPIXEL fractional accumulator so gentle tilts still creep smoothly.
 *
 *   velocity[1/16 px per frame] = tilt_counts * speed_pct / TILT_VEL_DEN
 *
 * With TILT_VEL_DEN = 400 a full sideways g (256) at 100% gives 4 px/frame.
 *
 * ACCEL_*_SIGN map the chip axes to screen axes (screen +x = right, +y =
 * down). Flip these on real hardware if the ball rolls the wrong way. */
#define TILT_DEADZONE       12      /* ignore small tilts (counts)           */
#define SUBPIXEL            16       /* fractional position resolution        */
#define TILT_VEL_DEN        400      /* velocity scaling denominator          */
#define ACCEL_X_SIGN        (+1)     /* screen vx = ACCEL_X_SIGN * accel_x    */
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
#define TICKS_PER_TENTH     10      /* 10 * 10ms = 100ms = 0.1 s             */
#define HURRY_UP_TENTHS     50      /* last 5.0 s -> blink the screen        */
#define BLINK_TOGGLE_MS     250     /* invert every 250ms => 2 swaps/second  */
#define LONGPRESS_MS        2000    /* hold S1 2 s to abort to the menu      */

/* ---- High scores ------------------------------------------------------- */
#define NUM_HIGHSCORES      3       /* the "big three"                       */
#define NAME_MAXLEN         12      /* up to 12 characters per name          */

/* ---- Colors (RGB565, from oledC_colors.h naming) ----------------------- */
#define COL_BG              0x0000  /* black background                      */
#define COL_WALL            0xFFFF  /* white walls                           */
#define COL_BALL            0xF800  /* red ball                              */
#define COL_START           0x07E0  /* green start marker                    */
#define COL_FINISH          0xFFE0  /* yellow finish marker                  */
#define COL_TEXT            0xFFFF  /* white text                            */
#define COL_TIMER           0x07FF  /* cyan timer                            */
#define COL_HILITE          0xFC00  /* orange menu highlight                 */

/* Difficulty record. */
typedef struct {
    uint16_t time_tenths;   /* allowed time, tenths of a second */
    uint8_t  speed_pct;     /* ball speed factor, percent       */
} Difficulty;

#endif /* GAME_CONFIG_H */
