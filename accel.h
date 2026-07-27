/*******************************************************************************
 * accel.h - ADXL345 accelerometer tilt reading for the Maze game.
 *
 * accel_init() brings the sensor into measure mode; accel_read_tilt() returns
 * the signed X and Y acceleration (the gravity vector produced by tilting the
 * board), used to roll the ball. Talks to I2C1 directly - see accel.c.
 ******************************************************************************/
#ifndef ACCEL_H
#define ACCEL_H

#include <stdbool.h>

/* Returns true if the ADXL345 was found (DEVID == 0xE5) and put in measure
 * mode. If false, the caller should show an I2C error. */
bool accel_init(void);

/* Reads the X and Y axes (signed, ~256 counts per g) in a single I2C burst.
 * tx / ty receive the tilt components. */
void accel_read_tilt(int *tx, int *ty);

#endif /* ACCEL_H */
