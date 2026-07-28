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
#define FRAME_MS        20          /* per-frame delay, plus draw time */

static uint32_t g_play_count = 0;

/* Difficulty table, indexed by (level-1). */
static const Difficulty DIFFS[3] = {
    { TIME_TENTHS_LVL1, SPEED_PCT_LVL1, BALL_R_BIG   },
    { TIME_TENTHS_LVL2, SPEED_PCT_LVL2, BALL_R_BIG   },
    { TIME_TENTHS_LVL3, SPEED_PCT_LVL3, BALL_R_SMALL },
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
typedef enum { ACT_START, ACT_SCORES, ACT_HELP } MenuAction;

/* Repaint one menu row. Only the touched rows are redrawn when the
 * selection moves, so scrolling the pot doesn't blink the whole screen. */
static void menu_item(uint8_t idx, bool sel)
{
    char buf[12];
    const char *txt;
    uint8_t y = (uint8_t)(24 + idx * 18);

    if (idx == 0) {
        sprintf(buf, "LEVEL %u", difficulty);
        txt = buf;
    } else {
        txt = (idx == 1) ? "START" : (idx == 2) ? "SCORES" : "HELP";
    }

    /* glyphs at scale 2 reach y+17, so the wipe band does too */
    oledC_DrawRectangle(0, y, SCREEN_W - 1, (uint8_t)(y + 17), COL_BG);
    draw_centered(y, 1, 2, txt, sel ? COL_HILITE : COL_TEXT);
    if (sel)
        oledC_DrawString(2, y, 1, 2, (uint8_t *)">", COL_HILITE);
}

static void menu_draw(uint8_t sel)
{
    uint8_t i;
    ui_clear();
    draw_centered(0, 2, 2, "MAZE", COL_TEXT);
    for (i = 0; i < 4; i++)
        menu_item(i, i == sel);
}

static MenuAction run_menu(void)
{
    uint8_t sel = 0, last = 0;
    menu_draw(0);
    for (;;) {
        input_poll(tick_ms());
        sel = input_pot_index(4);
        if (sel != last) {                        /* repaint only two rows */
            menu_item(last, false);
            menu_item(sel, true);
            last = sel;
        }

        if (input_s1_pressed() || input_s2_pressed()) {
            if (sel == 0) {                       /* cycle difficulty 1->2->3 */
                difficulty = (uint8_t)((difficulty % 3) + 1);
                menu_item(0, true);
                DELAY_milliseconds(150);   /* so one press can't cycle twice */
            } else if (sel == 1) {
                return ACT_START;
            } else if (sel == 2) {
                return ACT_SCORES;
            } else {
                return ACT_HELP;
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
    oledC_DrawRectangle(21, 19, 74, 19, COL_DIM);   /* underline the title */
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

/* ===================== Help screen ===================================== */
static void run_help(void)
{
    ui_clear();
    draw_centered(2, 1, 2, "HOW TO PLAY", COL_TEXT);
    oledC_DrawRectangle(15, 19, 80, 19, COL_DIM);
    oledC_DrawString(2, 24, 1, 1, (uint8_t *)"tilt = roll ball", COL_TEXT);
    oledC_DrawString(2, 34, 1, 1, (uint8_t *)"reach green door", COL_TEXT);
    oledC_DrawString(2, 44, 1, 1, (uint8_t *)"grab gold coins", COL_TEXT);
    oledC_DrawString(2, 56, 1, 1, (uint8_t *)"S1/S2 = pause", COL_HILITE);
    oledC_DrawString(2, 66, 1, 1, (uint8_t *)"hold S1 = menu", COL_HILITE);
    draw_centered(84, 1, 1, "press a key", COL_DIM);
    wait_any_key();
}

/* ===================== Coin counter (top-right) ======================== */
static void draw_coins(int collected)
{
    char buf[8];
    sprintf(buf, "%d", collected);
    oledC_DrawRectangle(76, 0, 95, 18, COL_BG);
    maze_redraw_region(76, 0, 95, 18);
    oledC_DrawRectangle(78, 7, 79, 8, COL_DOT);        /* a little coin */
    oledC_DrawString(83, 0, 1, 2, (uint8_t *)buf, COL_DOT);
}

/* ===================== Timer display =================================== */
static void draw_timer(uint16_t tenths, int16_t *last)
{
    char buf[8];
    if ((int16_t)tenths == *last) return;
    *last = (int16_t)tenths;
    sprintf(buf, "%u.%u", tenths / 10, tenths % 10);
    /* small background pad so old digits are covered (the glyphs reach row
     * 18 at this scale); then restore the wall pixels the pad erased, and
     * draw the digits on top */
    oledC_DrawRectangle(0, 0, 30, 18, COL_BG);
    maze_redraw_region(0, 0, 30, 18);
    oledC_DrawString(0, 0, 1, 2, (uint8_t *)buf, COL_DIM);
}

/* ===================== Gameplay ======================================== */
typedef enum { PLAY_WIN, PLAY_LOSE, PLAY_ABORT } PlayResult;

/* 3-2-1 before the ball unlocks, drawn over the maze. The digit pad is
 * erased between steps and the maze/dots/markers under it are repainted. */
static void run_countdown(void)
{
    char d[2];
    int i;
    for (i = 3; i >= 1; i--) {
        sprintf(d, "%d", i);
        draw_centered(34, 4, 4, d, COL_HILITE);
        DELAY_milliseconds(COUNTDOWN_STEP_MS);
        /* the scale-4 glyph covers x38..57, y38..69 */
        oledC_DrawRectangle(34, 36, 61, 71, COL_BG);
        maze_redraw_region(34, 36, 61, 71);
        maze_dots_redraw_region(34, 36, 61, 71);
        maze_draw_markers();            /* the finish room sits right there */
    }
}

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
    maze_dots_render();
    maze_start_pixel(&sx, &sy);
    ball_reset(sx, sy, d->ball_r);
    ball_draw_at(sx, sy);
    draw_coins(0);

    run_countdown();

    start_ms = tick_ms();       /* the clock starts after the countdown */
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

        /* A short press on either button pauses; another press resumes.
         * The clock stops while paused (start_ms is shifted on resume). */
        {
            bool p1 = input_s1_pressed(), p2 = input_s2_pressed();
            if (p1 || p2) {
            uint32_t paused_at = now;
            if (inverted) { inverted = false; display_normal(); }
            oledC_DrawRectangle(28, 36, 67, 57, COL_BG);
            draw_centered(38, 1, 2, "PAUSED", COL_TEXT);
            for (;;) {
                input_poll(tick_ms());
                if (input_s1_long_press()) {
                    display_normal();
                    return PLAY_ABORT;
                }
                if (input_s1_pressed() || input_s2_pressed())
                    break;
                DELAY_milliseconds(10);
            }
            oledC_DrawRectangle(28, 36, 67, 57, COL_BG);
            maze_redraw_region(28, 36, 67, 57);
            maze_dots_redraw_region(28, 36, 67, 57);
            maze_draw_markers();
            ball_draw_at(ball_x(), ball_y());
            draw_coins(maze_dots_total() - maze_dots_left());
            last_shown = -1;                  /* force a timer repaint */
            now = tick_ms();
            start_ms += now - paused_at;      /* don't count paused time */
            blink_ms += now - paused_at;
            }
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
        if (maze_dot_collect(ball_x(), ball_y(), ball_radius()))
            draw_coins(maze_dots_total() - maze_dots_left());
        ball_render();
        draw_timer(remaining, &last_shown);

        /* Keep the ball drawn on top (so the corner timer can't hide it). */
        ball_draw_at(ball_x(), ball_y());

        /* Reached the green door? */
        if (maze_at_finish(ball_x(), ball_y(), ball_radius())) {
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
    bool both_held = false;
    uint32_t both_since = 0;

    for (i = 0; i <= NAME_MAXLEN; i++) name[i] = 0;
    for (i = 0; i < NAME_MAXLEN; i++) name[i] = 'A';

    input_poll(tick_ms());
    (void)input_s1_pressed(); (void)input_s2_pressed();
    (void)input_s1_long_press();

    /* Static parts are drawn once; the loop below repaints only the name
     * line and the cursor, and only when they actually change. */
    ui_clear();
    draw_centered(4, 1, 2, "YOUR NAME", COL_TEXT);
    draw_centered(62, 1, 1, "hold S1 = skip", COL_DIM);
    draw_centered(72, 1, 1, "S1< S2> move", COL_DIM);
    draw_centered(84, 1, 1, "hold both = save", COL_DIM);

    for (;;) {
        input_poll(tick_ms());

        /* Holding S1 alone skips the scoreboard entry entirely. */
        if (!input_both_down() && input_s1_long_press()) {
            while (input_s1_down()) { input_poll(tick_ms()); DELAY_milliseconds(10); }
            (void)input_s1_pressed(); (void)input_s2_pressed();
            return;
        }

        /* Current letter chosen by the potentiometer. */
        name[cur] = (char)('A' + input_pot_index(26));

        /* Save when both keys stay held together for a moment (a brief
         * touch is treated as a cursor move, not a save). */
        if (input_both_down()) {
            if (!both_held) { both_held = true; both_since = tick_ms(); }
            if (tick_ms() - both_since >= SAVE_HOLD_MS) {
                name[maxpos + 1] = '\0';
                scoreboard_insert(name, score);
                /* wait for release, then drop the leftover press edges so
                 * the menu doesn't act on them */
                while (input_both_down()) { input_poll(tick_ms()); DELAY_milliseconds(10); }
                (void)input_s1_pressed(); (void)input_s2_pressed();
                return;
            }
        } else {
            both_held = false;
        }

        if (!input_both_down()) {
            if (input_s1_pressed()) {             /* move left  */
                if (cur > 0) cur--;
            }
            if (input_s2_pressed()) {             /* move right */
                if (cur < NAME_MAXLEN - 1) { cur++; if (cur > maxpos) maxpos = cur; }
            }
        }

        /* Repaint only when the visible state changed. Horizontal scale is
         * 1 so all NAME_MAXLEN characters (6 px each) fit on the panel. */
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

/* One thin rectangle outline built from four 1-px strips (DrawRectangle
 * fills, so an outline needs strips). */
static void draw_frame(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       uint16_t color)
{
    oledC_DrawRectangle(x0, y0, x1, y0, color);
    oledC_DrawRectangle(x0, y1, x1, y1, color);
    oledC_DrawRectangle(x0, y0, x0, y1, color);
    oledC_DrawRectangle(x1, y0, x1, y1, color);
}

static void run_result(PlayResult r, uint16_t score)
{
    char buf[24];
    int k;
    bool record;

    if (r == PLAY_LOSE) {
        ui_clear();
        draw_centered(22, 2, 2, "TIME UP", COL_TEXT);
        draw_centered(48, 1, 2, "Next time...", COL_TEXT);
        sprintf(buf, "coins %d/%d", maze_dots_total() - maze_dots_left(),
                maze_dots_total());
        draw_centered(68, 1, 1, buf, COL_DIM);
        draw_centered(84, 1, 1, "press a key", COL_DIM);
        wait_any_key();
        return;
    }

    /* WIN - a quick burst of expanding frames, then the text. */
    ui_clear();
    for (k = 4; k <= 40; k += 9) {
        draw_frame((uint8_t)(48 - k), (uint8_t)(48 - k),
                   (uint8_t)(47 + k), (uint8_t)(47 + k), COL_HILITE);
        DELAY_milliseconds(60);
        draw_frame((uint8_t)(48 - k), (uint8_t)(48 - k),
                   (uint8_t)(47 + k), (uint8_t)(47 + k), COL_BG);
    }
    draw_centered(30, 2, 2, "WELL", COL_TEXT);
    draw_centered(54, 2, 2, "DONE!", COL_TEXT);
    wait_any_key();

    /* A record means beating the current best (or being the first score). */
    record = scoreboard_qualifies(score) && score < scoreboard_score(0);

    ui_clear();
    draw_centered(16, 1, 2, "YOUR SCORE", COL_TEXT);
    sprintf(buf, "%u.%u", score / 10, score % 10);
    draw_centered(38, 2, 2, buf, COL_HILITE);
    sprintf(buf, "coins %d/%d", maze_dots_total() - maze_dots_left(),
            maze_dots_total());
    draw_centered(62, 1, 1, buf, COL_DIM);
    if (record)
        draw_centered(72, 1, 1, "NEW RECORD!", COL_HILITE);

    draw_centered(84, 1, 1, "press a key", COL_DIM);
    wait_any_key();
    if (scoreboard_qualifies(score))
        run_name_entry(score);
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
        } else if (a == ACT_HELP) {
            run_help();
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
