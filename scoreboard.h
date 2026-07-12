/*******************************************************************************
 * scoreboard.h - The "big three" high-score table.
 *
 * A score is the time taken to finish, in tenths of a second: LOWER IS BETTER.
 * The table keeps the three best (lowest) scores sorted best-first.
 ******************************************************************************/
#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "game_config.h"

typedef struct {
    char     name[NAME_MAXLEN + 1];
    uint16_t score_tenths;      /* time taken; lower is better */
} HighScore;

/* Reset to default placeholder entries. */
void scoreboard_init(void);

/* True if score_tenths would make it into the big three. */
bool scoreboard_qualifies(uint16_t score_tenths);

/* Insert a result, keeping the table sorted and capped at NUM_HIGHSCORES. */
void scoreboard_insert(const char *name, uint16_t score_tenths);

const char *scoreboard_name(int i);
uint16_t    scoreboard_score(int i);

#endif /* SCOREBOARD_H */
