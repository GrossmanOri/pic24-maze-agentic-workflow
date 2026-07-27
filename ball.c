/*******************************************************************************
 * ball.c - Ball physics, collision and rendering.
 ******************************************************************************/
#include "ball.h"
#include "game_config.h"
#include "maze.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_shapes.h"

static int cx, cy;          /* current center  */
static int prev_x, prev_y;  /* last drawn center */
static int subx, suby;      /* 1/SUBPIXEL fractional accumulators */

static int deadzone(int v)
{
    if (v > TILT_DEADZONE)  return v - TILT_DEADZONE;
    if (v < -TILT_DEADZONE) return v + TILT_DEADZONE;
    return 0;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void ball_reset(int x, int y)
{
    cx = prev_x = x;
    cy = prev_y = y;
    subx = suby = 0;
}

/* Step up to |n| pixels along x (dir = +/-1), stopping at a wall. */
static void move_x(int n)
{
    int dir = (n > 0) ? 1 : -1;
    int steps = (n > 0) ? n : -n;
    while (steps-- > 0) {
        if (maze_ball_collides(cx + dir, cy, BALL_RADIUS)) { subx = 0; break; }
        cx += dir;
    }
}

static void move_y(int n)
{
    int dir = (n > 0) ? 1 : -1;
    int steps = (n > 0) ? n : -n;
    while (steps-- > 0) {
        if (maze_ball_collides(cx, cy + dir, BALL_RADIUS)) { suby = 0; break; }
        cy += dir;
    }
}

void ball_update(int tx, int ty, uint8_t speed_pct)
{
    int sx, sy;
    int vx16, vy16;
    int maxv = BALL_MAX_STEP * SUBPIXEL;

    tx = deadzone(tx) * ACCEL_X_SIGN;
    ty = deadzone(ty) * ACCEL_Y_SIGN;

    /* velocity in 1/SUBPIXEL pixels per frame (32-bit product: tilt can reach
     * ~512 counts, and 512*120 overflows the 16-bit int of the PIC24) */
    vx16 = clampi((int)((int32_t)tx * speed_pct / TILT_VEL_DEN), -maxv, maxv);
    vy16 = clampi((int)((int32_t)ty * speed_pct / TILT_VEL_DEN), -maxv, maxv);

    subx += vx16;
    suby += vy16;

    sx = subx / SUBPIXEL; subx -= sx * SUBPIXEL;
    sy = suby / SUBPIXEL; suby -= sy * SUBPIXEL;

    if (sx) move_x(sx);
    if (sy) move_y(sy);
}

/* The ball with a little glint pixel so it reads as a sphere, not a blob. */
void ball_draw_at(int x, int y)
{
    oledC_DrawCircle((uint8_t)x, (uint8_t)y, BALL_RADIUS, COL_BALL);
    oledC_DrawPoint((uint8_t)(x - 1), (uint8_t)(y - 1), COL_TEXT);
}

void ball_render(void)
{
    if (cx == prev_x && cy == prev_y)
        return;                         /* nothing moved -> no repaint */

    /* Erase the ball at its old position by repainting it in the background
     * color, then draw it at the new one. */
    oledC_DrawCircle((uint8_t)prev_x, (uint8_t)prev_y, BALL_RADIUS, COL_BG);
    /* Restore markers and any dot the erase may have clipped. */
    maze_draw_markers();
    maze_dots_redraw_region(prev_x - BALL_RADIUS - 1, prev_y - BALL_RADIUS - 1,
                            prev_x + BALL_RADIUS + 1, prev_y + BALL_RADIUS + 1);
    ball_draw_at(cx, cy);

    prev_x = cx;
    prev_y = cy;
}

int ball_x(void) { return cx; }
int ball_y(void) { return cy; }
