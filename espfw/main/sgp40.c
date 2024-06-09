/* Talking to SGP40 VOC (volatile organic compounds) sensors */

#include <driver/i2c.h>
#include <esp_log.h>
#include "sgp40.h"
#include "sdkconfig.h"
#include "settings.h"

#define SGP40ADDR 0x59

#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

static i2c_port_t sgp40i2cport;

void sgp40_init(void)
{
    if (settings.sgp40_i2cport > 0) {
      /* An I2C-port is configured, but is that port disabled? */
      if ((settings.i2c_n_scl[settings.sgp40_i2cport - 1] == 0)
       || (settings.i2c_n_sda[settings.sgp40_i2cport - 1] == 0)) {
        /* It is. Force disabled-setting for us too. */
        settings.sgp40_i2cport = 0;
        ESP_LOGW("sgp40.c", "WARNING: SGP40 automatically disabled because it is connected to a disabled I2C port.");
      }
    }
    if (settings.sgp40_i2cport == 0) return;
    sgp40i2cport = ((settings.sgp40_i2cport == 1) ? I2C_NUM_0 : I2C_NUM_1);
}

/* This function is based on Sensirons example code and datasheet
 * for the SHT3x and was written for that. CRC-calculation is
 * exactly the same for the SGP40, so we reuse it. */
static uint8_t sgp40_crc(uint8_t b1, uint8_t b2)
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

void sgp40_startmeasraw(float temp, float hum)
{
    if (settings.sgp40_i2cport == 0) return;
    uint16_t humenc = hum * 65535.0 / 100.0;
    uint16_t tempenc = (temp + 45.0) * 65535.0 / 175.0;
    uint8_t cmd[8] = { 0x26, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    /* Fill in temp, RH, and CRC */
    cmd[2] = humenc >> 8; cmd[3] = humenc & 0xff;
    cmd[4] = sgp40_crc(cmd[2], cmd[3]);
    cmd[5] = tempenc >> 8; cmd[6] = tempenc & 0xff;
    cmd[7] = sgp40_crc(cmd[5], cmd[6]);
    ESP_LOGI("sgp40.c", "Will send: (%02x %02x = cmd) (%02x %02x = hum) %02x (%02x %02x = temp) %02x",
                        cmd[0], cmd[1], cmd[2], cmd[3],
                        cmd[4], cmd[5], cmd[6], cmd[7]);
    esp_err_t res = i2c_master_write_to_device(sgp40i2cport, SGP40ADDR,
                                               cmd, sizeof(cmd),
                                               pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    if (res != ESP_OK) {
      ESP_LOGE("sgp40.c", "ERROR: sending start-measurement-command to SGP40 failed with error '%s'.",
                          esp_err_to_name(res));
      return;
    }
}

void sgp40_read(struct sgp40data * d)
{
    uint8_t readbuf[3];
    d->valid = 0; d->vocraw = 0xffff;
    if (settings.sgp40_i2cport == 0) return;
    int res = i2c_master_read_from_device(sgp40i2cport, SGP40ADDR,
                                          readbuf, sizeof(readbuf),
                                          I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (res != ESP_OK) {
      ESP_LOGE("sgp40.c", "ERROR: I2C-read from SGP40 failed with error '%s'.",
                          esp_err_to_name(res));
      return;
    }
    /* Check CRC */
    if (sgp40_crc(readbuf[0], readbuf[1]) != readbuf[2]) {
      ESP_LOGE("sgp40.c", "ERROR: CRC-check for read failed.");
      return;
    }
    //ESP_LOGI("sgp40.c", "Read VOC: %x %x", readbuf[0], readbuf[1]);
    /* OK, CRC matches, this is looking good. */
    d->vocraw = (readbuf[0] << 8) | readbuf[1];
    /* Mark the result as valid. */
    d->valid = 1;
}

