/* Talking to SEN50 particulate matter sensors */

#include <driver/i2c.h>
#include <esp_log.h>
#include "sen50.h"
#include "sdkconfig.h"
#include "settings.h"


#define SEN50ADDR 0x69 /* This is fixed. */

#define I2C_MASTER_TIMEOUT_MS 100  /* Timeout for I2C communication */

static i2c_port_t sen50i2cport;

void sen50_init(void)
{
  if (settings.sen50_i2cport > 0) {
    /* An I2C-port is configured, but is that port disabled? */
    if ((settings.i2c_n_scl[settings.sen50_i2cport - 1] == 0)
     || (settings.i2c_n_sda[settings.sen50_i2cport - 1] == 0)) {
      /* It is. Force disabled-setting for us too. */
      settings.sen50_i2cport = 0;
      ESP_LOGW("sen50.c", "WARNING: SEN50 automatically disabled because it is connected to a disabled I2C port.");
    }
  }
  if (settings.sen50_i2cport == 0) return;
  sen50i2cport = ((settings.sen50_i2cport == 1) ? I2C_NUM_0 : I2C_NUM_1);

  /* The default power-on-config of the sensor should
   * be perfectly fine for us, so there is nothing to
   * configure here. */
}

void sen50_startmeas(void)
{
    if (settings.sen50_i2cport == 0) return;
    uint8_t cmd[2] = { 0x00, 0x21 };
    esp_err_t res = i2c_master_write_to_device(sen50i2cport, SEN50ADDR,
                                               cmd, sizeof(cmd),
                                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (res != ESP_OK) {
      ESP_LOGE("sen50.c", "ERROR: sending start-measurement-command to SEN50 failed with error '%s'.",
                          esp_err_to_name(res));
      return;
    }
}

void sen50_stopmeas(void)
{
    if (settings.sen50_i2cport == 0) return;
    uint8_t cmd[2] = { 0x01, 0x04 };
    esp_err_t res = i2c_master_write_to_device(sen50i2cport, SEN50ADDR,
                                               cmd, sizeof(cmd),
                                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (res != ESP_OK) {
      ESP_LOGE("sen50.c", "ERROR: sending stop-measurement-command to SEN50 failed with error '%s'.",
                          esp_err_to_name(res));
      return;
    }
}

/* This function is based on Sensirons example code and datasheet
 * for the SHT3x and was written for that. CRC-calculation is
 * exactly the same for the SEN50, so we reuse it. */
static uint8_t sen50_crc(uint8_t b1, uint8_t b2)
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

void sen50_read(struct sen50data * d)
{
    d->valid = 0;
    d->pm010raw = 0xffff;  d->pm025raw = 0xffff; d->pm040raw = 0xffff; d->pm100raw = 0xffff;
    d->pm010 = -999.99; d->pm025 = -999.9; d->pm040 = -999.99; d->pm100 = -999.9;
    if (settings.sen50_i2cport == 0) return;
    uint8_t readbuf[23];
    uint8_t cmd[2] = { 0x03, 0xc4 };
    i2c_master_write_to_device(sen50i2cport, SEN50ADDR,
                               cmd, sizeof(cmd),
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /* Datasheet says we need to give the sensor at least 20 ms time before
     * we can read the data so that it can fill its internal buffers */
    vTaskDelay(pdMS_TO_TICKS(22));
    int res = i2c_master_read_from_device(sen50i2cport, SEN50ADDR,
                                          readbuf, sizeof(readbuf),
                                          I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (res != ESP_OK) {
      ESP_LOGE("sen50.c", "ERROR: I2C-read from SEN50 failed with error '%s'.",
                          esp_err_to_name(res));
      return;
    }
    /* Check CRC */
    if (sen50_crc(readbuf[0], readbuf[1]) != readbuf[2]) {
      ESP_LOGE("sen50.c", "ERROR: CRC-check for read part 1 failed.");
      return;
    }
    if (sen50_crc(readbuf[3], readbuf[4]) != readbuf[5]) {
      ESP_LOGE("sen50.c", "ERROR: CRC-check for read part 2 failed.");
      return;
    }
    if (sen50_crc(readbuf[6], readbuf[7]) != readbuf[8]) {
      ESP_LOGE("sen50.c", "ERROR: CRC-check for read part 3 failed.");
      return;
    }
    if (sen50_crc(readbuf[9], readbuf[10]) != readbuf[11]) {
      ESP_LOGE("sen50.c", "ERROR: CRC-check for read part 4 failed.");
      return;
    }
    /* We could also check CRC for temperature / humidity / noxi data, but
     * that doesn't contain valid values anyways (_should_ only contain
     * 0xffff on our sensor because it doesn't measure those) and we
     * throw that away anyways, so why should we care? */
    /* OK, CRC matches, this is looking good. */
    d->pm010raw = (readbuf[0] << 8) | readbuf[1];
    d->pm025raw = (readbuf[3] << 8) | readbuf[4];
    d->pm040raw = (readbuf[6] << 8) | readbuf[7];
    d->pm100raw = (readbuf[9] << 8) | readbuf[10];
    /* Check for 0xffff, as that is the poweron-value of the register,
     * and never a valid reading. */
    if ((d->pm010raw == 0xffff) || (d->pm025raw == 0xffff)
     || (d->pm040raw == 0xffff) || (d->pm100raw == 0xffff)) {
      ESP_LOGE("sen50.c", "ERROR: poweron-value in data register.");
      return;
    }
    d->pm010 = ((float)d->pm010raw / 10.0);
    d->pm025 = ((float)d->pm025raw / 10.0);
    d->pm040 = ((float)d->pm040raw / 10.0);
    d->pm100 = ((float)d->pm100raw / 10.0);
    /* Mark the result as valid. */
    d->valid = 1;
}

