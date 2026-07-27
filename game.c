/*******************************************************************************
 * game.c - Maze game controller: menu, gameplay, timer, win/lose, scoring and
 * name entry.
 *
 * The whole game is a state machine driven from game_run(). The OLED is only
 * ever touched here (and in the modules called from here) in the main context,
 * so there is no contention with the Timer1 tick ISR.
 ******************************************************************************/
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "game_config.h"
#include "ticktimer.h"
#include "input.h"
#include "accel.h"
#include "maze.h"
#include "ball.h"
#include "scoreboard.h"

#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

/* Maze seed varies with difficulty and replay count, so every game is a
 * different (still fair, solvable) maze - and each difficulty looks distinct. */
#define MAZE_SEED_BASE  0x00C0FFEEu
#define FRAME_MS        30          /* per-frame delay, plus draw time */

static uint32_t g_play_count = 0;

/* Difficulty table, indexed by (level-1). */
static const Difficulty DIFFS[3] = {
    { TIME_TENTHS_LVL1, SPEED_PCT_LVL1 },
    { TIME_TENTHS_LVL2, SPEED_PCT_LVL2 },
    { TIME_TENTHS_LVL3, SPEED_PCT_LVL3 },
};

static uint8_t difficulty = 1;      /* 1..3 */

/* ===================== Small UI helpers ================================= */
static void ui_clear(void)
{
    oledC_setBackground(COL_BG);
    oledC_clearScreen();
}

static void display_normal(void)
{
    /* 0xA6 = normal display. (0xA4 blanks the panel - don't use it here.) */
    oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_ON, 0, 0);
}
static void display_inverse(void)
{
    oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_INVERSE, 0, 0);   /* 0xA7 = inverse */
}

/* Draw a string horizontally centered at row y. */
static void draw_centered(uint8_t y, uint8_t sx, uint8_t sy,
                          const char *s, uint16_t color)
{
    int len = (int)strlen(s);
    int w = len * (5 * sx + 1) - 1;
    int x = (SCREEN_W - w) / 2;
    if (x < 0) x = 0;
    oledC_DrawString((uint8_t)x, y, sx, sy, (uint8_t *)s, color);
}

/* Wait until either button is pressed. */
static void wait_any_key(void)
{
    /* swallow stale edges */
    input_poll(tick_ms());
    (void)input_s1_pressed(); (void)input_s2_pressed();
    for (;;) {
        input_poll(tick_ms());
        if (input_s1_pressed() || input_s2_pressed()) return;
        DELAY_milliseconds(10);
    }
}

/* ===================== Menu ============================================= */
typedef enum { ACT_START, ACT_SCORES } MenuAction;

static void menu_draw(uint8_t sel)
{
    char buf[16];
    ui_clear();
    draw_centered(2, 2, 2, "MAZE", COL_TEXT);

    sprintf(buf, "DIFF %d", difficulty);
    draw_centered(34, 1, 2, buf,      sel == 0 ? COL_HILITE : COL_TEXT);
    draw_centered(54, 1, 2, "START",  sel == 1 ? COL_HILITE : COL_TEXT);
    draw_centered(74, 1, 2, "SCORES", sel == 2 ? COL_HILITE : COL_TEXT);
}

static MenuAction run_menu(void)
{
    uint8_t sel = 0, last = 0xFF;
    menu_draw(0);
    for (;;) {
        input_poll(tick_ms());
        sel = input_pot_index(3);
        if (sel != last) { menu_draw(sel); last = sel; }

        if (input_s1_pressed() || input_s2_pressed()) {
            if (sel == 0) {                       /* cycle difficulty 1->2->3 */
                difficulty = (uint8_t)((difficulty % 3) + 1);
                menu_draw(sel);
                DELAY_milliseconds(150);   /* so one press can't cycle twice */
            } else if (sel == 1) {
                return ACT_START;
            } else {
                return ACT_SCORES;
            }
        }
        DELAY_milliseconds(10);
    }
}

/* ===================== High scores ===================================== */
static void run_highscores(void)
{
    char buf[24];
    int i;
    ui_clear();
    draw_centered(2, 1, 2, "BIG THREE", COL_TEXT);
    for (i = 0; i < NUM_HIGHSCORES; i++) {
        uint16_t s = scoreboard_score(i);
        /* names show truncated to 8 chars so the line fits the 96px panel */
        if (s >= 9999)
            sprintf(buf, "%d %-8.8s --", i + 1, scoreboard_name(i));
        else
            sprintf(buf, "%d %-8.8s %u.%u", i + 1, scoreboard_name(i),
                    s / 10, s % 10);
        oledC_DrawString(2, (uint8_t)(28 + i * 18), 1, 2, (uint8_t *)buf, COL_TEXT);
    }
    draw_centered(86, 1, 1, "press a key", COL_DIM);
    wait_any_key();
}

/* ===================== Timer display =================================== */
static void draw_timer(uint16_t tenths, int16_t *last)
{
    char buf[8];
    if ((int16_t)tenths == *last) return;
    *last = (int16_t)tenths;
    sprintf(buf, "%u.%u", tenths / 10, tenths % 10);
    /* small background pad so old digits are covered; then restore the wall
     * pixels the pad erased, and draw the digits on top */
    oledC_DrawRectangle(0, 0, 30, 16, COL_BG);
    maze_redraw_region(0, 0, 30, 16);
    oledC_DrawString(0, 0, 1, 2, (uint8_t *)buf, COL_DIM);
}

/* ===================== Gameplay ======================================== */
typedef enum { PLAY_WIN, PLAY_LOSE, PLAY_ABORT } PlayResult;

static PlayResult run_play(uint16_t *score_out)
{
    const Difficulty *d = &DIFFS[difficulty - 1];
    int sx, sy, tx, ty;
    uint32_t start_ms, now, blink_ms = 0;
    uint16_t remaining;
    int16_t  last_shown = -1;
    bool inverted = false;

    /* Build and draw the maze. Grid size grows with difficulty (denser/harder),
     * and the seed varies per difficulty and per replay (different every game). */
    {
        int grid = (difficulty == 1) ? GRID_LVL1 :
                   (difficulty == 2) ? GRID_LVL2 : GRID_LVL3;
        g_play_count++;
        /* big odd multipliers just keep the seeds far apart */
        maze_generate(MAZE_SEED_BASE + difficulty * 130003u +
                      g_play_count * 49999u, grid, grid);
    }
    maze_render();
    maze_start_pixel(&sx, &sy);
    ball_reset(sx, sy);
    oledC_DrawCircle((uint8_t)sx, (uint8_t)sy, BALL_RADIUS, COL_BALL);

    start_ms = tick_ms();
    /* prime input edges */
    input_poll(start_ms);
    (void)input_s1_pressed(); (void)input_s2_pressed(); (void)input_s1_long_press();

    for (;;) {
        now = tick_ms();
        input_poll(now);

        /* Long press S1 (2s) aborts the game back to the menu. Every exit
         * path restores the normal display in case the blink left it inverted. */
        if (input_s1_long_press()) {
            display_normal();
            return PLAY_ABORT;
        }

        /* Time bookkeeping. */
        {
            uint32_t elapsed_tenths = (now - start_ms) / 100u;
            if (elapsed_tenths >= d->time_tenths) {
                display_normal();
                return PLAY_LOSE;
            }
            remaining = (uint16_t)(d->time_tenths - elapsed_tenths);
        }

        /* Physics + render. */
        accel_read_tilt(&tx, &ty);
        ball_update(tx, ty, d->speed_pct);
        ball_render();
        draw_timer(remaining, &last_shown);

        /* Keep the ball drawn on top (so the corner timer can't hide it). */
        oledC_DrawCircle((uint8_t)ball_x(), (uint8_t)ball_y(), BALL_RADIUS, COL_BALL);

        /* Finish reached? */
        if (maze_at_finish(ball_x(), ball_y())) {
            display_normal();
            *score_out = (uint16_t)((now - start_ms) / 100u);
            return PLAY_WIN;
        }

        /* Last 5 seconds: blink inverse at 2 swaps/second. */
        if (remaining <= HURRY_UP_TENTHS) {
            if (now - blink_ms >= BLINK_TOGGLE_MS) {
                blink_ms = now;
                inverted = !inverted;
                if (inverted) display_inverse(); else display_normal();
            }
        } else if (inverted) {
            inverted = false; display_normal();
        }

        DELAY_milliseconds(FRAME_MS);
    }
}

/* ===================== Result + name entry ============================= */
static void run_name_entry(uint16_t score)
{
    char name[NAME_MAXLEN + 1];
    int cur = 0, maxpos = 0, i;
    int last_cur = -1, last_maxpos = -1;
    char last_letter = 0;

    for (i = 0; i <= NAME_MAXLEN; i++) name[i] = 0;
    for (i = 0; i < NAME_MAXLEN; i++) name[i] = 'A';

    input_poll(tick_ms());
    (void)input_s1_pressed(); (void)input_s2_pressed();

    /* Static parts are drawn once; the loop below repaints only the name
     * line and the cursor, and only when they actually change. */
    ui_clear();
    draw_centered(4, 1, 2, "YOUR NAME", COL_TEXT);
    draw_centered(82, 1, 1, "S1< S2> S1+S2=ok", COL_DIM);

    for (;;) {
        input_poll(tick_ms());

        /* Current letter chosen by the potentiometer. */
        name[cur] = (char)('A' + input_pot_index(26));

        /* Save when both keys are held together. */
        if (input_both_down()) {
            name[maxpos + 1] = '\0';
            scoreboard_insert(name, score);
            /* wait for release, then drop the leftover press edges so the
             * menu doesn't act on them */
            while (input_both_down()) { input_poll(tick_ms()); DELAY_milliseconds(10); }
            (void)input_s1_pressed(); (void)input_s2_pressed();
            return;
        }

        if (input_s1_pressed()) {                 /* move left  */
            if (cur > 0) cur--;
        }
        if (input_s2_pressed()) {                 /* move right */
            if (cur < NAME_MAXLEN - 1) { cur++; if (cur > maxpos) maxpos = cur; }
        }

        /* Repaint only when the visible state changed. Text scale is 1 so
         * all NAME_MAXLEN characters fit on the 96px panel (6 px each). */
        if (name[cur] != last_letter || cur != last_cur || maxpos != last_maxpos) {
            char disp[NAME_MAXLEN + 2];
            int w  = (maxpos + 1) * 6 - 1;
            int x0 = (SCREEN_W - w) / 2;

            last_letter = name[cur];
            last_cur = cur; last_maxpos = maxpos;

            for (i = 0; i <= maxpos; i++) disp[i] = name[i];
            disp[maxpos + 1] = '\0';

            oledC_DrawRectangle(0, 38, SCREEN_W - 1, 62, COL_BG);
            draw_centered(40, 1, 2, disp, COL_TEXT);
            /* cursor underline at the current position */
            {
                int cxs = x0 + cur * 6;
                oledC_DrawLine((uint8_t)cxs, 58, (uint8_t)(cxs + 5), 58, 1, COL_HILITE);
            }
        }
        DELAY_milliseconds(30);
    }
}

static void run_result(PlayResult r, uint16_t score)
{
    char buf[20];

    if (r == PLAY_LOSE) {
        ui_clear();
        draw_centered(24, 2, 2, "TIME UP", COL_TEXT);
        draw_centered(50, 1, 2, "Next time...", COL_TEXT);
        draw_centered(82, 1, 1, "press a key", COL_DIM);
        wait_any_key();
        return;
    }

    /* WIN */
    ui_clear();
    draw_centered(30, 2, 2, "WELL", COL_TEXT);
    draw_centered(54, 2, 2, "DONE!", COL_TEXT);
    wait_any_key();

    ui_clear();
    draw_centered(20, 1, 2, "YOUR SCORE", COL_TEXT);
    sprintf(buf, "%u.%u", score / 10, score % 10);
    draw_centered(44, 2, 2, buf, COL_HILITE);

    if (scoreboard_qualifies(score)) {
        draw_centered(72, 1, 1, "TOP 3! press key", COL_DIM);
        wait_any_key();
        run_name_entry(score);
    } else {
        draw_centered(72, 1, 1, "press a key", COL_DIM);
        wait_any_key();
    }
}

/* ===================== Public entry ==================================== */
static bool accel_ready = false;

void game_init(void)
{
    scoreboard_init();
    input_init();
    tick_init();
    /* The accelerometer is initialized on the first START rather than at
     * boot, so a bad I2C connection doesn't stop the menu from showing. */
    ui_clear();
}

void game_run(void)
{
    for (;;) {
        MenuAction a = run_menu();
        if (a == ACT_SCORES) {
            run_highscores();
        } else {                    /* ACT_START */
            uint16_t score = 0;
            PlayResult r;

            /* Lazy, non-fatal accelerometer init. */
            if (!accel_ready)
                accel_ready = accel_init();
            if (!accel_ready) {
                ui_clear();
                draw_centered(28, 2, 2, "I2C ERR", COL_BALL);
                draw_centered(54, 1, 1, "Accel not found", COL_TEXT);
                wait_any_key();
                continue;           /* back to menu, no hang */
            }

            r = run_play(&score);
            if (r != PLAY_ABORT)
                run_result(r, score);
        }
    }
}
