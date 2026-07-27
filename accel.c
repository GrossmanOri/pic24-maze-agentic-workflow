/*******************************************************************************
 * accel.c - ADXL345 tilt reading over I2C.
 *
 * Bytes are sent with a local i2c_tx() instead of i2c1_driver_TXData(): the
 * driver's TRSTAT wait let bytes get lost at this clock (register writes
 * never reached the chip), so i2c_tx() waits on MI2C1IF, which is only set
 * after the full byte + ACK. A NACK is also waited out before STOP for the
 * same reason. The transmit and NACK waits are bounded by a guard counter.
 *
 * The six data registers (0x32..0x37) are read in one burst; per-register
 * reads can tear the 16-bit values.
 ******************************************************************************/
#include <stdint.h>
#include <xc.h>
#include "accel.h"
#include "Accel_i2c.h"
#include "i2cDriver/i2c1_driver.h"
#include "System/delay.h"

#define ACCEL_ADDR_W      0x3A
#define ADXL345_DEVID     0xE5
#define REG_DEVID         0x00
#define REG_DATA_FORMAT   0x31
#define REG_POWER_CTL     0x2D
#define REG_DATAX0        0x32
#define POWER_CTL_MEASURE 0x08

/* Worst case some tens of ms; a full I2C byte at 100kHz needs only ~90us. */
#define I2C_GUARD         50000u

/* Single-byte transmit (see file header). */
static void i2c_tx(unsigned char b)
{
    uint16_t guard;

    /* Never overwrite a byte that is still queued. */
    for (guard = I2C_GUARD; I2C1STATbits.TBF && guard; guard--) ;

    IFS1bits.MI2C1IF = 0;
    I2C1TRN = b;

    /* MI2C1IF sets after the 9th clock (byte + ACK fully on the wire). */
    for (guard = I2C_GUARD; !IFS1bits.MI2C1IF && guard; guard--) ;
}

/* Send a NACK and wait for it to finish before the caller issues a STOP. */
static void i2c_drain_nack(void)
{
    uint16_t guard;
    i2c1_driver_sendNACK();
    for (guard = I2C_GUARD; I2C1CONLbits.ACKEN && guard; guard--) ;
}

/* Raw single-register write. */
static void accel_write_reg(unsigned char reg, unsigned char val)
{
    i2c1_driver_start();
    i2c_tx(ACCEL_ADDR_W);
    i2c_tx(reg);
    i2c_tx(val);
    i2c1_driver_stop();
}

/* Raw single-register read. */
static unsigned char accel_read_reg(unsigned char reg)
{
    unsigned char v;
    i2c1_driver_start();
    i2c_tx(ACCEL_ADDR_W);
    i2c_tx(reg);
    i2c1_driver_restart();
    i2c_tx(ACCEL_ADDR_W | 1);
    i2c1_driver_startRX();
    i2c1_driver_waitRX();
    v = (unsigned char)i2c1_driver_getRXData();
    i2c_drain_nack();
    i2c1_driver_stop();
    return v;
}

bool accel_init(void)
{
    unsigned char id;
    int tries;

    i2c1_open();
    DELAY_milliseconds(5);

    id = accel_read_reg(REG_DEVID);
    if (id != ADXL345_DEVID)
        return false;

    accel_write_reg(REG_DATA_FORMAT, 0x00);   /* +/-2g, right justified */
    DELAY_milliseconds(2);

    /* Enter measure mode; verify the write actually stuck. */
    for (tries = 0; tries < 8; tries++) {
        accel_write_reg(REG_POWER_CTL, POWER_CTL_MEASURE);
        DELAY_milliseconds(10);
        if (accel_read_reg(REG_POWER_CTL) == POWER_CTL_MEASURE)
            return true;
    }
    return false;
}

void accel_read_tilt(int *tx, int *ty)
{
    unsigned char d[6];
    int i;

    i2c1_driver_start();
    i2c_tx(ACCEL_ADDR_W);                    /* write address  */
    i2c_tx(REG_DATAX0);                      /* point at X0    */
    i2c1_driver_restart();
    i2c_tx(ACCEL_ADDR_W | 1);                /* read address   */

    for (i = 0; i < 6; i++) {
        i2c1_driver_startRX();
        i2c1_driver_waitRX();
        d[i] = (unsigned char)i2c1_driver_getRXData();
        if (i < 5) i2c1_driver_sendACK();
        else       i2c_drain_nack();
    }
    i2c1_driver_stop();

    *tx = (int)(int16_t)(((uint16_t)d[1] << 8) | d[0]);   /* X */
    *ty = (int)(int16_t)(((uint16_t)d[3] << 8) | d[2]);   /* Y */
}
