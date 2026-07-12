/*******************************************************************************
 * game.h - Top-level game controller (menu + play + scoring state machine).
 ******************************************************************************/
#ifndef GAME_H
#define GAME_H

void game_init(void);   /* one-time init of all subsystems */
void game_run(void);     /* never returns: runs the state machine */

#endif /* GAME_H */
