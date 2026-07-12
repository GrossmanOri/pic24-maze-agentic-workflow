/*******************************************************************************
 * scoreboard.c - High-score table (lower time = better).
 ******************************************************************************/
#include "scoreboard.h"

#define SCORE_EMPTY  9999       /* sentinel "no score yet" (very bad time) */

static HighScore table[NUM_HIGHSCORES];

static void str_copy(char *dst, const char *src, int max)
{
    int i = 0;
    if (src) for (; i < max && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

void scoreboard_init(void)
{
    int i;
    for (i = 0; i < NUM_HIGHSCORES; i++) {
        str_copy(table[i].name, "---", NAME_MAXLEN);
        table[i].score_tenths = SCORE_EMPTY;
    }
}

bool scoreboard_qualifies(uint16_t score_tenths)
{
    /* Qualifies if better than the worst (last) entry. */
    return score_tenths < table[NUM_HIGHSCORES - 1].score_tenths;
}

void scoreboard_insert(const char *name, uint16_t score_tenths)
{
    int i, pos;

    if (!scoreboard_qualifies(score_tenths))
        return;

    /* Find insertion position (ascending by score). */
    pos = NUM_HIGHSCORES - 1;
    for (i = 0; i < NUM_HIGHSCORES; i++) {
        if (score_tenths < table[i].score_tenths) { pos = i; break; }
    }

    /* Shift worse entries down by one. */
    for (i = NUM_HIGHSCORES - 1; i > pos; i--)
        table[i] = table[i - 1];

    str_copy(table[pos].name, name, NAME_MAXLEN);
    table[pos].score_tenths = score_tenths;
}

const char *scoreboard_name(int i)  { return table[i].name; }
uint16_t    scoreboard_score(int i) { return table[i].score_tenths; }
