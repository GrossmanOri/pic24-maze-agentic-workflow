/*******************************************************************************
 * maze.c - Maze generation, rendering and collision.
 *
 * The grid size (columns x rows) is chosen per difficulty at run time, so
 * harder levels use a denser maze. Arrays are sized to the maximum grid.
 ******************************************************************************/
#include "maze.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_shapes.h"

/* ---- Logical walls (sized to the max grid) ----------------------------- */
static uint8_t vwall[MAZE_ROWS][MAZE_COLS + 1];
static uint8_t hwall[MAZE_ROWS + 1][MAZE_COLS];

/* ---- Active grid (set by maze_generate) -------------------------------- */
static int g_cols = MAZE_COLS;
static int g_rows = MAZE_ROWS;
static int g_cellx = SCREEN_W / MAZE_COLS;
static int g_celly = SCREEN_H / MAZE_ROWS;

/* Start cell (bottom-center) and central 2x2 finish room, derived from grid. */
static int scol(void) { return g_cols / 2; }
static int srow(void) { return g_rows - 1; }
static int cc0(void)  { return g_cols / 2 - 1; }
static int cc1(void)  { return g_cols / 2; }
static int cr0(void)  { return g_rows / 2 - 1; }
static int cr1(void)  { return g_rows / 2; }

/* ---- Collision bitmap (96 x 96, 1 bit per pixel) ----------------------- */
#define BMP_STRIDE  ((SCREEN_W + 7) / 8)
static uint8_t wallbits[SCREEN_H][BMP_STRIDE];

/* ---- Deterministic PRNG (LCG) ------------------------------------------ */
static uint32_t rng_state;
static void     rng_seed(uint32_t s) { rng_state = s ? s : 1u; }
static uint16_t rng_next(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (uint16_t)((rng_state >> 16) & 0x7FFFu);
}

/* ---- Pixel coordinate of a grid line ----------------------------------- */
static int xline(int c) { return (c < g_cols) ? c * g_cellx : (SCREEN_W - 1); }
static int yline(int r) { return (r < g_rows) ? r * g_celly : (SCREEN_H - 1); }

static void maze_finish_pixel(int *x, int *y);   /* center of the finish room */

/* ---- Bitmap helpers ---------------------------------------------------- */
static void bmp_set(int x, int y)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    wallbits[y][x >> 3] |= (uint8_t)(1u << (x & 7));
}

bool maze_is_wall_px(int x, int y)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return true;
    return (wallbits[y][x >> 3] >> (x & 7)) & 1u;
}

static void rasterize_walls(void)
{
    int r, c, x, y;

    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < BMP_STRIDE; x++)
            wallbits[y][x] = 0;

    for (r = 0; r < g_rows; r++)
        for (c = 0; c <= g_cols; c++)
            if (vwall[r][c]) {
                int xx = xline(c);
                for (y = yline(r); y <= yline(r + 1); y++) bmp_set(xx, y);
            }

    for (r = 0; r <= g_rows; r++)
        for (c = 0; c < g_cols; c++)
            if (hwall[r][c]) {
                int yy = yline(r);
                for (x = xline(c); x <= xline(c + 1); x++) bmp_set(x, yy);
            }
}

/* ---- Maze generation: recursive backtracker (iterative) ---------------- */
static void remove_wall_between(int c0, int r0, int c1, int r1)
{
    if (c1 == c0 + 1) vwall[r0][c0 + 1] = 0;
    else if (c1 == c0 - 1) vwall[r0][c0] = 0;
    else if (r1 == r0 + 1) hwall[r0 + 1][c0] = 0;
    else if (r1 == r0 - 1) hwall[r0][c0] = 0;
}

/* Carve one maze from a seed (walls + finish room), without rasterizing. */
static void gen_walls(uint32_t seed)
{
    static uint8_t visited[MAZE_ROWS][MAZE_COLS];
    static uint8_t stackC[MAZE_ROWS * MAZE_COLS];
    static uint8_t stackR[MAZE_ROWS * MAZE_COLS];
    int sp = 0;
    int r, c;

    for (r = 0; r < g_rows; r++)
        for (c = 0; c <= g_cols; c++) vwall[r][c] = 1;
    for (r = 0; r <= g_rows; r++)
        for (c = 0; c < g_cols; c++) hwall[r][c] = 1;
    for (r = 0; r < g_rows; r++)
        for (c = 0; c < g_cols; c++) visited[r][c] = 0;

    rng_seed(seed);

    c = scol(); r = srow();
    visited[r][c] = 1;
    stackC[sp] = (uint8_t)c; stackR[sp] = (uint8_t)r; sp++;

    while (sp > 0) {
        int nc[4], nr[4], n = 0;
        if (c > 0          && !visited[r][c-1]) { nc[n]=c-1; nr[n]=r; n++; }
        if (c < g_cols-1   && !visited[r][c+1]) { nc[n]=c+1; nr[n]=r; n++; }
        if (r > 0          && !visited[r-1][c]) { nc[n]=c; nr[n]=r-1; n++; }
        if (r < g_rows-1   && !visited[r+1][c]) { nc[n]=c; nr[n]=r+1; n++; }

        if (n > 0) {
            int k = rng_next() % n;
            int ncx = nc[k], nry = nr[k];
            remove_wall_between(c, r, ncx, nry);
            visited[nry][ncx] = 1;
            stackC[sp] = (uint8_t)ncx; stackR[sp] = (uint8_t)nry; sp++;
            c = ncx; r = nry;
        } else {
            sp--;
            if (sp > 0) { c = stackC[sp-1]; r = stackR[sp-1]; }
        }
    }

    /* Open the central 2x2 finish chamber (a clear room in the middle). */
    vwall[cr0()][cc1()] = 0;
    vwall[cr1()][cc1()] = 0;
    hwall[cr1()][cc0()] = 0;
    hwall[cr1()][cc1()] = 0;
}

/* Shortest start->finish path length in cells (BFS on the logical walls);
 * -1 if unreachable (cannot happen for a backtracker maze). */
static int solution_path_len(void)
{
    static int16_t dist[MAZE_ROWS][MAZE_COLS];   /* path can exceed 127 cells */
    static uint8_t qc[MAZE_ROWS * MAZE_COLS], qr[MAZE_ROWS * MAZE_COLS];
    int head = 0, tail = 0, r, c, best = -1;

    for (r = 0; r < g_rows; r++)
        for (c = 0; c < g_cols; c++) dist[r][c] = -1;

    c = scol(); r = srow();
    dist[r][c] = 1;
    qc[tail] = (uint8_t)c; qr[tail] = (uint8_t)r; tail++;

    while (head < tail) {
        int d;
        c = qc[head]; r = qr[head]; head++;
        d = dist[r][c];
        if (c > 0        && !vwall[r][c]   && dist[r][c-1] < 0)
            { dist[r][c-1] = (int16_t)(d+1); qc[tail]=(uint8_t)(c-1); qr[tail]=(uint8_t)r; tail++; }
        if (c < g_cols-1 && !vwall[r][c+1] && dist[r][c+1] < 0)
            { dist[r][c+1] = (int16_t)(d+1); qc[tail]=(uint8_t)(c+1); qr[tail]=(uint8_t)r; tail++; }
        if (r > 0        && !hwall[r][c]   && dist[r-1][c] < 0)
            { dist[r-1][c] = (int16_t)(d+1); qc[tail]=(uint8_t)c; qr[tail]=(uint8_t)(r-1); tail++; }
        if (r < g_rows-1 && !hwall[r+1][c] && dist[r+1][c] < 0)
            { dist[r+1][c] = (int16_t)(d+1); qc[tail]=(uint8_t)c; qr[tail]=(uint8_t)(r+1); tail++; }
    }

    for (r = cr0(); r <= cr1(); r++)
        for (c = cc0(); c <= cc1(); c++)
            if (dist[r][c] > 0 && (best < 0 || dist[r][c] < best))
                best = dist[r][c];
    return best;
}

/* Number of dead-end cells (exactly 3 surrounding walls). */
static int dead_end_count(void)
{
    int r, c, n = 0;
    for (r = 0; r < g_rows; r++)
        for (c = 0; c < g_cols; c++) {
            int w = 0;
            if (vwall[r][c])     w++;
            if (vwall[r][c + 1]) w++;
            if (hwall[r][c])     w++;
            if (hwall[r + 1][c]) w++;
            if (w == 3) n++;
        }
    return n;
}

/* One backtracker run can end up with a very short start->finish route, so
 * carve up to MAZE_TRIES seed variants and keep the longest route that still
 * has at least MIN_DEAD_ENDS dead ends, stopping early once the route covers
 * half of all cells. */
#define MAZE_TRIES      16
#define MIN_DEAD_ENDS   5

void maze_generate(uint32_t seed, int cols, int rows)
{
    uint32_t best_seed = seed, most_ends_seed = seed;
    int best_len = -1, most_ends = -1, t;

    if (cols < 2) cols = 2; if (cols > MAZE_COLS) cols = MAZE_COLS;
    if (rows < 2) rows = 2; if (rows > MAZE_ROWS) rows = MAZE_ROWS;
    g_cols = cols; g_rows = rows;
    g_cellx = SCREEN_W / g_cols;
    g_celly = SCREEN_H / g_rows;

    for (t = 0; t < MAZE_TRIES; t++) {
        uint32_t s = seed + (uint32_t)t * 9973u;   /* spread the try seeds */
        int len, ends, target = (g_cols * g_rows) / 2;

        gen_walls(s);
        ends = dead_end_count();
        if (ends > most_ends) { most_ends = ends; most_ends_seed = s; }
        if (ends < MIN_DEAD_ENDS)
            continue;
        len = solution_path_len();
        if (len > best_len) { best_len = len; best_seed = s; }
        if (len >= target)
            break;                    /* long, winding route - good enough */
    }

    /* If no try had enough dead ends (does not really happen on these grid
     * sizes), fall back to the one that came closest. */
    if (best_len < 0) best_seed = most_ends_seed;

    gen_walls(best_seed);             /* rebuild the winner (cheap) */
    rasterize_walls();
}

/* ---- Rendering --------------------------------------------------------- */
void maze_draw_markers(void)
{
    int sx, sy, fx, fy;
    maze_start_pixel(&sx, &sy);
    maze_finish_pixel(&fx, &fy);
    oledC_DrawCircle((uint8_t)sx, (uint8_t)sy, 2, COL_START);
    oledC_DrawRectangle((uint8_t)(fx - 3), (uint8_t)(fy - 3),
                        (uint8_t)(fx + 3), (uint8_t)(fy + 3), COL_FINISH);
}

void maze_render(void)
{
    int r, c;

    oledC_setBackground(COL_BG);
    oledC_clearScreen();

    /* oledC_DrawLine can't draw vertical lines (its loop is x<x1), so the
     * walls are drawn as 1-px DrawRectangle strips instead. */
    for (r = 0; r < g_rows; r++)
        for (c = 0; c <= g_cols; c++)
            if (vwall[r][c]) {
                int xx = xline(c);
                oledC_DrawRectangle((uint8_t)xx, (uint8_t)yline(r),
                                    (uint8_t)xx, (uint8_t)yline(r + 1), COL_WALL);
            }
    for (r = 0; r <= g_rows; r++)
        for (c = 0; c < g_cols; c++)
            if (hwall[r][c]) {
                int yy = yline(r);
                oledC_DrawRectangle((uint8_t)xline(c), (uint8_t)yy,
                                    (uint8_t)xline(c + 1), (uint8_t)yy, COL_WALL);
            }

    maze_draw_markers();
}

/* Repaint any wall pixels inside a rectangle (from the collision bitmap).
 * Used after UI overlays (timer pad) erase part of the maze. */
void maze_redraw_region(int x0, int y0, int x1, int y1)
{
    int x, y;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
    if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (maze_is_wall_px(x, y))
                oledC_DrawPoint((uint8_t)x, (uint8_t)y, COL_WALL);
}

/* ---- Collision --------------------------------------------------------- */
bool maze_ball_collides(int cx, int cy, int r)
{
    int dx, dy;
    if (cx - r < 0 || cx + r >= SCREEN_W || cy - r < 0 || cy + r >= SCREEN_H)
        return true;
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r)
                if (maze_is_wall_px(cx + dx, cy + dy))
                    return true;
    return false;
}

/* ---- Cell helpers ------------------------------------------------------ */
void maze_start_pixel(int *x, int *y)
{
    *x = (xline(scol()) + xline(scol() + 1)) / 2;
    *y = (yline(srow()) + yline(srow() + 1)) / 2;
}

static void maze_finish_pixel(int *x, int *y)
{
    *x = (xline(cc0()) + xline(cc1() + 1)) / 2;
    *y = (yline(cr0()) + yline(cr1() + 1)) / 2;
}

bool maze_at_finish(int cx, int cy)
{
    return cx >= xline(cc0()) && cx <= xline(cc1() + 1) &&
           cy >= yline(cr0()) && cy <= yline(cr1() + 1);
}

