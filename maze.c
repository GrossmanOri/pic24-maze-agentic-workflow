/*******************************************************************************
 * maze.c - Maze generation, rendering and collision.
 ******************************************************************************/
#include "maze.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_shapes.h"

/* ---- Logical walls -----------------------------------------------------
 * vwall[r][c] : vertical wall on the LEFT edge of cell (c,r), c = 0..COLS
 * hwall[r][c] : horizontal wall on the TOP edge of cell (c,r), r = 0..ROWS
 * Border edges (c==0, c==COLS, r==0, r==ROWS) are always walls.            */
static uint8_t vwall[MAZE_ROWS][MAZE_COLS + 1];
static uint8_t hwall[MAZE_ROWS + 1][MAZE_COLS];

/* ---- Collision bitmap (96 x 96, 1 bit per pixel) ----------------------- */
#define BMP_STRIDE  ((SCREEN_W + 7) / 8)        /* 12 bytes per row */
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
static int xline(int c) { return (c < MAZE_COLS) ? c * CELL_PX : (SCREEN_W - 1); }
static int yline(int r) { return (r < MAZE_ROWS) ? r * CELL_PX : (SCREEN_H - 1); }

/* ---- Bitmap helpers ---------------------------------------------------- */
static void bmp_set(int x, int y)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    wallbits[y][x >> 3] |= (uint8_t)(1u << (x & 7));
}

bool maze_is_wall_px(int x, int y)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return true; /* OOB */
    return (wallbits[y][x >> 3] >> (x & 7)) & 1u;
}

/* ---- Rasterize the logical walls into the collision bitmap -------------- */
static void rasterize_walls(void)
{
    int r, c, x, y;

    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < BMP_STRIDE; x++)
            wallbits[y][x] = 0;

    /* Vertical walls: a line at x=xline(c) spanning cell row r. */
    for (r = 0; r < MAZE_ROWS; r++)
        for (c = 0; c <= MAZE_COLS; c++)
            if (vwall[r][c]) {
                int xx = xline(c);
                for (y = yline(r); y <= yline(r + 1); y++) bmp_set(xx, y);
            }

    /* Horizontal walls: a line at y=yline(r) spanning cell col c. */
    for (r = 0; r <= MAZE_ROWS; r++)
        for (c = 0; c < MAZE_COLS; c++)
            if (hwall[r][c]) {
                int yy = yline(r);
                for (x = xline(c); x <= xline(c + 1); x++) bmp_set(x, yy);
            }
}

/* ---- Maze generation: recursive backtracker (iterative) ---------------- */
static void remove_wall_between(int c0, int r0, int c1, int r1)
{
    if (c1 == c0 + 1) vwall[r0][c0 + 1] = 0;        /* moved right */
    else if (c1 == c0 - 1) vwall[r0][c0] = 0;       /* moved left  */
    else if (r1 == r0 + 1) hwall[r0 + 1][c0] = 0;   /* moved down  */
    else if (r1 == r0 - 1) hwall[r0][c0] = 0;       /* moved up    */
}

void maze_generate(uint32_t seed)
{
    static uint8_t visited[MAZE_ROWS][MAZE_COLS];
    static uint8_t stackC[MAZE_ROWS * MAZE_COLS];
    static uint8_t stackR[MAZE_ROWS * MAZE_COLS];
    int sp = 0;
    int r, c;

    /* All walls present, nothing visited. */
    for (r = 0; r < MAZE_ROWS; r++)
        for (c = 0; c <= MAZE_COLS; c++) vwall[r][c] = 1;
    for (r = 0; r <= MAZE_ROWS; r++)
        for (c = 0; c < MAZE_COLS; c++) hwall[r][c] = 1;
    for (r = 0; r < MAZE_ROWS; r++)
        for (c = 0; c < MAZE_COLS; c++) visited[r][c] = 0;

    rng_seed(seed);

    c = START_COL; r = START_ROW;
    visited[r][c] = 1;
    stackC[sp] = (uint8_t)c; stackR[sp] = (uint8_t)r; sp++;

    while (sp > 0) {
        int nc[4], nr[4], n = 0;
        /* Collect unvisited orthogonal neighbours. */
        if (c > 0           && !visited[r][c-1]) { nc[n]=c-1; nr[n]=r; n++; }
        if (c < MAZE_COLS-1 && !visited[r][c+1]) { nc[n]=c+1; nr[n]=r; n++; }
        if (r > 0           && !visited[r-1][c]) { nc[n]=c; nr[n]=r-1; n++; }
        if (r < MAZE_ROWS-1 && !visited[r+1][c]) { nc[n]=c; nr[n]=r+1; n++; }

        if (n > 0) {
            int k = rng_next() % n;
            int ncx = nc[k], nry = nr[k];
            remove_wall_between(c, r, ncx, nry);
            visited[nry][ncx] = 1;
            stackC[sp] = (uint8_t)ncx; stackR[sp] = (uint8_t)nry; sp++;
            c = ncx; r = nry;
        } else {
            sp--;                       /* backtrack */
            if (sp > 0) { c = stackC[sp-1]; r = stackR[sp-1]; }
        }
    }

    rasterize_walls();
}

/* ---- Rendering --------------------------------------------------------- */
void maze_draw_markers(void)
{
    int sx, sy, fx, fy;
    maze_start_pixel(&sx, &sy);
    maze_finish_pixel(&fx, &fy);
    oledC_DrawCircle((uint8_t)sx, (uint8_t)sy, 2, COL_START);
    oledC_DrawCircle((uint8_t)fx, (uint8_t)fy, 2, COL_FINISH);
}

void maze_render(void)
{
    int r, c;

    oledC_setBackground(COL_BG);
    oledC_clearScreen();

    /* Vertical walls. */
    for (r = 0; r < MAZE_ROWS; r++)
        for (c = 0; c <= MAZE_COLS; c++)
            if (vwall[r][c]) {
                int xx = xline(c);
                oledC_DrawLine((uint8_t)xx, (uint8_t)yline(r),
                               (uint8_t)xx, (uint8_t)yline(r + 1), 1, COL_WALL);
            }
    /* Horizontal walls. */
    for (r = 0; r <= MAZE_ROWS; r++)
        for (c = 0; c < MAZE_COLS; c++)
            if (hwall[r][c]) {
                int yy = yline(r);
                oledC_DrawLine((uint8_t)xline(c), (uint8_t)yy,
                               (uint8_t)xline(c + 1), (uint8_t)yy, 1, COL_WALL);
            }

    maze_draw_markers();
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
    *x = (xline(START_COL) + xline(START_COL + 1)) / 2;
    *y = (yline(START_ROW) + yline(START_ROW + 1)) / 2;
}

void maze_finish_pixel(int *x, int *y)
{
    *x = (xline(FINISH_COL) + xline(FINISH_COL + 1)) / 2;
    *y = (yline(FINISH_ROW) + yline(FINISH_ROW + 1)) / 2;
}

bool maze_at_finish(int cx, int cy)
{
    return cx >= xline(FINISH_COL) && cx <= xline(FINISH_COL + 1) &&
           cy >= yline(FINISH_ROW) && cy <= yline(FINISH_ROW + 1);
}

bool maze_vwall(int row, int col) { return vwall[row][col] != 0; }
bool maze_hwall(int row, int col) { return hwall[row][col] != 0; }
