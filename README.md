# Maze Ball Game — Final Project

A ball-in-maze game for the **PIC24FJ256GA705 Curiosity** board. Tilt the board
to roll a ball (read with the **ADXL345** accelerometer) from START to FINISH of
a 96×96 maze on the **OLED**, within a time limit set by the chosen difficulty.

Built on the course **accelerometer-lab template** (System / oledDriver /
spiDriver / i2cDriver), as required, so it compiles unchanged with XC16.

---

## Build & run

* Open `Maze.X` in MPLAB X (device PIC24FJ256GA705, tool *Starter Kits (PKOB)*,
  compiler XC16 v2.10), then **Clean and Build** → **Make and Program Device**.
* Hardware: **OLED C Click → socket A**, **Accel Click → socket B**.
* Verified with XC16 v2.10: all 16 translation units compile `-Wall` clean and
  link (≈128 KB program, ≈1.5 KB RAM).

## Controls

| Control | Action |
|---|---|
| Potentiometer | scroll menu / pick a letter in name entry |
| S1 | select / move name cursor **left** |
| S2 | select / move name cursor **right** |
| S1 + S2 | save name |
| Hold S1 (2 s) | abort game → main menu |

See `OPERATING_INSTRUCTIONS.txt` for full gameplay.

---

## Source layout

| File | Responsibility |
|---|---|
| `main.c` | entry: `SYSTEM_Initialize` → `game_init` → `game_run` |
| `game.c/.h` | state machine: menu, gameplay, win/lose, score, name entry, blink |
| `game_config.h` | all tunable constants (timing, physics, colors, maze size) |
| `maze.c/.h` | maze generation, wall rendering, pixel-collision bitmap |
| `ball.c/.h` | ball physics (tilt→velocity), per-axis wall collision, draw/erase |
| `accel.c/.h` | ADXL345 tilt vector (X/Y) via a single I²C burst read |
| `input.c/.h` | potentiometer (ADC AN12) + S1/S2 buttons (debounce, long-press) |
| `ticktimer.c/.h` | Timer1 10 ms tick → millisecond time base |
| `scoreboard.c/.h` | "Big Three" high-score table (lower time = better) |
| *template* | `System/`, `oledDriver/`, `spiDriver/`, `i2cDriver/`, `Accel_i2c.*` |

## Hardware pin map (PIC24FJ256GA705 Curiosity)

| Function | Pin(s) |
|---|---|
| OLED (SPI1) | SDI=RB13, SDO=RB14, SCK=RB15; CS=RC9, DC=RC3, RST=RA13, EN=RC8 |
| Accelerometer | I²C1 (ADXL345, write addr `0x3A`, DEVID `0xE5`) |
| Potentiometer | pin RB12 = ADC channel **AN8** |
| Button S1 | RA11 (active-low, pull-up) |
| Button S2 | RA12 (active-low, pull-up) |

---

## How the spec maps to the code

* **Difficulty 1–3** (time + speed factor) → `DIFFS[]` in `game.c`, constants in
  `game_config.h`. Speed factor is an integer percent (80/100/120) to avoid
  floating point.
* **Timer** in tenths, top-left, real-time → `draw_timer()`; time base from the
  Timer1 tick (`ticktimer.c`).
* **Tilt → motion**, per-axis, speed scaled by difficulty → `ball_update()`
  (fixed-point: a 1/16-pixel accumulator lets gentle tilts creep smoothly).
* **Walls block the ball; ball stays in the window** → `maze_ball_collides()`
  tests the ball disc against a rasterized 96×96 wall bitmap; the outer border
  is always walled. Motion steps one pixel at a time so thin walls can't be
  tunneled.
* **Ball = filled circle, erase-then-draw** → `ball_render()` (draws the old
  position in the background color, then the new one — project guidance #5).
* **Walls = 1-px lines** → drawn with `oledC_DrawLine` from the wall model
  (guidance #3).
* **Last 5 s blink** (normal↔inverse, 2/s) → `display_inverse/normal` in the
  play loop.
* **Long-press S1 (2 s) aborts to menu** → `input_s1_long_press()`.
* **Win/Lose screens, score, name entry, Big Three** → `run_result()`,
  `run_name_entry()`, `scoreboard.c`.

## Maze design

The maze is built once by a deterministic recursive-backtracker
(`maze_generate`, fixed seed `0x00C0FFEE`), so it is a **fixed, fair,
guaranteed-solvable** maze with many dead ends. A host test (see *Testing*)
confirms a ball of radius 3 can travel START→FINISH and that there are ≥5 dead
ends (this maze has 7). `S` = start, `F` = finish:

```
+---+---+---+---+---+---+---+---+
| S |           |       |       |
+   +   +---+   +   +   +   +   +
|       |       |   |       |   |
+---+---+   +---+---+   +---+   +
|       |           |   |       |
+   +---+---+---+   +   +   +---+
|               |   |   |       |
+   +---+   +---+   +---+---+   +
|       |       |           |   |
+---+   +---+   +---+---+   +   +
|   |       |               |   |
+   +---+   +---+---+---+---+   +
|       |       |       |       |
+   +   +---+   +   +   +---+   +
|   |               |         F |
+---+---+---+---+---+---+---+---+
```

## Testing

Two layers, both reproducible:

1. **Firmware build** — `xc16-gcc -mcpu=24FJ256GA705 -DFCY=4000000` compiles
   every file `-Wall` clean and links to a valid ELF.
2. **Host logic test** — `maze.c`, `ball.c`, `scoreboard.c` compiled natively
   against mock OLED stubs verify: maze solvable by the ball (pixel BFS), ≥5
   dead ends, sealed border, ball never overlaps a wall while moving, dead-zone
   behavior, and high-score sorting/qualifying. All pass.

## On-hardware tuning

Everything playable is in `game_config.h`. The **one** thing that may need a
flip on real hardware is the tilt direction — if the ball rolls the wrong way,
change `ACCEL_X_SIGN` / `ACCEL_Y_SIGN` (and/or swap axes). Ball feel:
`TILT_VEL_DEN` (higher = calmer), `TILT_DEADZONE`, `BALL_MAX_STEP`.
