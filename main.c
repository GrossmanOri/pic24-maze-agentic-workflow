/*******************************************************************************
 * main.c - Maze game entry point (PIC24FJ256GA705 Curiosity board).
 *
 * Final project: tilt the board to roll a ball through a 96x96 maze from START
 * to FINISH before time runs out. Built on the accelerometer-lab template
 * (System / oledDriver / spiDriver / i2cDriver).
 *
 *   SYSTEM_Initialize()  - clock, pins, OLED (SPI1) from the template
 *   game_init()          - input (pot + S1/S2), Timer1 tick, accelerometer
 *   game_run()           - menu / gameplay / scoring state machine (loops)
 ******************************************************************************/
#include "System/system.h"
#include "game.h"

int main(void)
{
    SYSTEM_Initialize();
    game_init();
    game_run();          /* never returns */
    return 0;
}
