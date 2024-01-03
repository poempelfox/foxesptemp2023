/* Talking to SCD41 CO2 sensors */

#include <driver/i2c.h>
#include <esp_log.h>
#include "scd41.h"
#include "sdkconfig.h"
#include "settings.h"


#define SCD41ADDR 0x62

#define I2C_MASTER_TIMEOUT_MS 100  /* Timeout for I2C communication */

static i2c_port_t scd41i2cport;

void scd41_init(void)
{
    if (settings.scd41_i2cport > 0) {
      /* An I2C-port is configured, but is that port disabled? */
      if ((settings.i2c_n_scl[settings.scd41_i2cport - 1] == 0)
       || (settings.i2c_n_sda[settings.scd41_i2cport - 1] == 0)) {
        /* It is. Force disabled-setting for us too. */
        settings.scd41_i2cport = 0;
        ESP_LOGW("scd41.c", "WARNING: SCD41 automatically disabled because it is connected to a disabled I2C port.");
      }
    }
    if (settings.scd41_i2cport == 0) return;
    scd41i2cport = ((settings.scd41_i2cport == 1) ? I2C_NUM_0 : I2C_NUM_1);

    /* The default power-on-config of the sensor should
     * be perfectly fine for us, so there is nothing to
     * configure here. */
}

void scd41_startmeas(void)
{
    if (settings.scd41_i2cport == 0) return;
    uint8_t cmd[2] = { 0x21, 0xac };
    i2c_master_write_to_device(scd41i2cport, SCD41ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /* We ignore the return value. If that failed, we'll notice
     * soon enough, namely when we try to read the result... */
}

void scd41_stopmeas(void)
{
    if (settings.scd41_i2cport == 0) return;
    uint8_t cmd[2] = { 0x3f, 0x86 };
    i2c_master_write_to_device(scd41i2cport, SCD41ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /* We ignore the return value. If that failed, we'll notice
     * soon enough, namely when we try to read the result... */
}

/* This function is based on Sensirons example code and datasheet
 * for the SHT3x and was written for that. CRC-calculation is
 * exactly the same for the SCD41, so we reuse it. */
static uint8_t scd41_crc(uint8_t b1, uint8_t b2)
{
    uint8_t crc = 0xff; /* Start value */
    uint8_t b;
    crc ^= b1;
    for (b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x131;
      } else {
        crc = crc << 1;
      }
    }
    crc ^= b2;
    for (b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x131;
      } else {
        crc = crc << 1;
      }
    }
    return crc;
}

void scd41_read(struct scd41data * d)
{
    d->valid = 0;
    d->co2 = 0xffff;  d->tempraw = 0xffff; d->humraw = 0xffff;
    d->temp = -999.9; d->hum = -999.99;
    if (settings.scd41_i2cport == 0) return;
    uint8_t readbuf[9];
    uint8_t cmd[2] = { 0xec, 0x05 };
    i2c_master_write_to_device(scd41i2cport, SCD41ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /* Datasheet says we need to give the sensor at least 1 ms time before
     * we can read the data */
    vTaskDelay(pdMS_TO_TICKS(2));
    int res = i2c_master_read_from_device(scd41i2cport, SCD41ADDR,
                                          readbuf, sizeof(readbuf),
                                          I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (res != ESP_OK) {
      ESP_LOGE("scd41.c", "ERROR: I2C-read from SCD41 failed.");
      return;
    }
    /* Check CRC */
    if (scd41_crc(readbuf[0], readbuf[1]) != readbuf[2]) {
      ESP_LOGE("scd41.c", "ERROR: CRC-check for read part 1 failed.");
      return;
    }
    if (scd41_crc(readbuf[3], readbuf[4]) != readbuf[5]) {
      ESP_LOGE("scd41.c", "ERROR: CRC-check for read part 2 failed.");
      return;
    }
    if (scd41_crc(readbuf[6], readbuf[7]) != readbuf[8]) {
      ESP_LOGE("scd41.c", "ERROR: CRC-check for read part 3 failed.");
      return;
    }
    /* OK, CRC matches, this is looking good. */
    d->co2 = (readbuf[0] << 8) | readbuf[1];
    d->tempraw = (readbuf[3] << 8) | readbuf[4];
    d->humraw = (readbuf[6] << 8) | readbuf[7];
    if ((d->co2 == 0xffff) || (d->tempraw == 0xffff) || (d->humraw == 0xffff)) {
      ESP_LOGE("scd41.c", "ERROR: SCD41 reported at least one value as unknown.");
      return;
    }
    d->temp = ((float)d->tempraw * 175.0 / 65535.0) - 45.0;
    d->hum = ((float)d->humraw * 100.0 / 65535.0);
    /* Mark the result as valid. */
    d->valid = 1;
}

