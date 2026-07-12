/*******************************************************************************
 * accel.c - ADXL345 tilt reading for the Maze game.
 *
 * IMPORTANT: register writes use the RAW i2c1_driver primitives (start/TX/stop)
 * instead of i2cWriteSlave(). i2cWriteSlave() aborts the moment it thinks it
 * saw a NACK, which on this board left POWER_CTL unwritten (POWER_CTL read back
 * 0 -> sensor stuck in standby -> data registers all 0 -> ball never moved).
 * The raw write always sends the data byte, exactly like the proven burst read.
 *
 * The six data registers (0x32..0x37) are read in ONE I2C burst (they are
 * double-buffered, so per-byte reads lose the high byte). We use X and Y.
 ******************************************************************************/
#include <stdint.h>
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

volatile unsigned char g_accel_powerctl = 0xEE;   /* read-back for diagnostics */

/* Raw single-register write (does not abort on a spurious NACK). */
static void accel_write_reg(unsigned char reg, unsigned char val)
{
    i2c1_driver_start();
    i2c1_driver_TXData(ACCEL_ADDR_W);
    i2c1_driver_TXData(reg);
    i2c1_driver_TXData(val);
    i2c1_driver_stop();
}

/* Raw single-register read. */
static unsigned char accel_read_reg(unsigned char reg)
{
    unsigned char v;
    i2c1_driver_start();
    i2c1_driver_TXData(ACCEL_ADDR_W);
    i2c1_driver_TXData(reg);
    i2c1_driver_restart();
    i2c1_driver_TXData(ACCEL_ADDR_W | 1);
    i2c1_driver_startRX();
    i2c1_driver_waitRX();
    v = (unsigned char)i2c1_driver_getRXData();
    i2c1_driver_sendNACK();
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

    /* Enter measure mode; verify it stuck (raw write + raw read-back). */
    for (tries = 0; tries < 8; tries++) {
        accel_write_reg(REG_POWER_CTL, POWER_CTL_MEASURE);
        DELAY_milliseconds(10);
        g_accel_powerctl = accel_read_reg(REG_POWER_CTL);
        if (g_accel_powerctl == POWER_CTL_MEASURE)
            break;
    }
    return true;
}

void accel_read_tilt(int *tx, int *ty)
{
    unsigned char d[6];
    int i;

    i2c1_driver_start();
    i2c1_driver_TXData(ACCEL_ADDR_W);        /* write address  */
    i2c1_driver_TXData(REG_DATAX0);          /* point at X0    */
    i2c1_driver_restart();
    i2c1_driver_TXData(ACCEL_ADDR_W | 1);    /* read address   */

    for (i = 0; i < 6; i++) {
        i2c1_driver_startRX();
        i2c1_driver_waitRX();
        d[i] = (unsigned char)i2c1_driver_getRXData();
        if (i < 5) i2c1_driver_sendACK();
        else       i2c1_driver_sendNACK();
    }
    i2c1_driver_stop();

    *tx = (int)(int16_t)(((uint16_t)d[1] << 8) | d[0]);   /* X */
    *ty = (int)(int16_t)(((uint16_t)d[3] << 8) | d[2]);   /* Y */
}
