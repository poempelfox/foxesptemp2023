/* Talking to the LPS35HW pressure sensor */

#include <esp_log.h>
#include <driver/i2c.h>
#include "lps35hw.h"
#include "sdkconfig.h"
#include "settings.h"


/* The LPS35HW can respond to address 0x5c or 0x5d, depending on whether
 * one of its pins is pulled high or low, so the address may vary
 * between different breakout-boards. */
#define LPS35HWBASEADDR 0x5c
#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

static i2c_port_t lps35hwi2cport;
static uint8_t lps35hwaddr;

static esp_err_t lps35hw_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(lps35hwi2cport, lps35hwaddr,
                                        &reg_addr, 1, data, len,
                                        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
}

static esp_err_t lps35hw_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2] = {reg_addr, data};

    ret = i2c_master_write_to_device(lps35hwi2cport, lps35hwaddr,
                                     write_buf, sizeof(write_buf),
                                     pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));

    return ret;
}

void lps35hw_init(void)
{
    if (settings.lps35hw_i2cport > 0) {
      /* An I2C-port is configured, but is that port disabled? */
      if ((settings.i2c_n_scl[settings.lps35hw_i2cport - 1] == 0)
       || (settings.i2c_n_sda[settings.lps35hw_i2cport - 1] == 0)) {
        /* It is. Force disabled-setting for us too. */
        settings.lps35hw_i2cport = 0;
        ESP_LOGW("lps35hw.c", "WARNING: LPS35HW automatically disabled because it is connected to a disabled I2C port.");
      }
    }
    if (settings.lps35hw_i2cport == 0) return;
    lps35hwi2cport = ((settings.sht4x_i2cport == 1) ? I2C_NUM_0 : I2C_NUM_1);
    lps35hwaddr = LPS35HWBASEADDR + settings.lps35hw_addr;

    /* Configure the LPS35HW */
    /* Other than the LPS25HB which did NOT support a oneshot-mode
     * and thus had to be configured to do continous measurements
     * at the lowest possible rate, the LPS35HW can do one-shot.
     * So we do not need to configure anything here right now,
     * the default values in all control registers are perfectly
     * fine. */
}

void lps35hw_startmeas(void)
{
    if (settings.lps35hw_i2cport == 0) return;
    /* CTRL_REG2 0x11: IF_ADD_INC (bit 4), ONE_SHOT (bit 0) */
    lps35hw_register_write_byte(0x11, (0x10 | 0x01));
}

double lps35hw_readpressure(void)
{
    if (settings.lps35hw_i2cport == 0) {
      return -999999.9;
    }
    uint8_t prr[3];
    if (lps35hw_register_read(0x80 | 0x28, &prr[0], 3) != ESP_OK) {
      /* There was an I2C read error - return a negative pressure to signal that. */
      return -999999.9;
    }
    double press = (((uint32_t)prr[2]  << 16)
                  + ((uint32_t)prr[1]  <<  8)
                  + ((uint32_t)prr[0]  <<  0)) / 4096.0;
    return press;
}

