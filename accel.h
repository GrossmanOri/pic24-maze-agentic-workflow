/*******************************************************************************
 * accel.h - ADXL345 accelerometer tilt reading for the Maze game.
 *
 * Wraps the Accel_i2c API. accel_init() brings the sensor into measure mode;
 * accel_read_tilt() returns the signed X and Y acceleration (the gravity
 * vector produced by tilting the board), used to roll the ball.
 ******************************************************************************/
#ifndef ACCEL_H
#define ACCEL_H

#include <stdbool.h>

/* POWER_CTL read-back after measure-mode init (0x08 means measure mode is on).
 * Exposed for the on-screen diagnostic. */
extern volatile unsigned char g_accel_powerctl;

/* Returns true if the ADXL345 was found (DEVID == 0xE5) and put in measure
 * mode. If false, the caller should show an I2C error. */
bool accel_init(void);

/* Reads the X and Y axes (signed, ~256 counts per g) in a single I2C burst.
 * tx / ty receive the tilt components. */
void accel_read_tilt(int *tx, int *ty);

#endif /* ACCEL_H */
