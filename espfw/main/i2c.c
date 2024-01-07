
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include "settings.h"

static uint32_t settingtoi2cclock(uint8_t s)
{
    switch (s) {
    case 1: return   25000U; // 25 kHz - there should be no need to use this.
    case 2: return  200000U; // 200 kHz - half the 'fast mode' of 400kHz
    case 3: return  400000U; // 400 kHz - what NXP calls 'fast mode'
    case 4: return 1000000U; // 1000 kHz - the maximum the ESP32 can do.
    };
    return 100000U; /* We use a medium-speed default. */
}

void i2c_port_init(void)
{
  for (int i2cp = 0; i2cp <= 1; i2cp++) {
    if ((settings.i2c_n_scl[i2cp] != 0) && (settings.i2c_n_sda[i2cp] != 0)) {
      i2c_config_t i2cpnconf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (settings.i2c_n_sda[i2cp] - 1),
        .scl_io_num = (settings.i2c_n_scl[i2cp] - 1),
        .sda_pullup_en = ((settings.i2c_n_pullups[i2cp])
                          ? GPIO_PULLUP_ENABLE
                          : GPIO_PULLUP_DISABLE),
        .scl_pullup_en = ((settings.i2c_n_pullups[i2cp])
                          ? GPIO_PULLUP_ENABLE
                          : GPIO_PULLUP_DISABLE),
        .master.clk_speed = settingtoi2cclock(settings.i2c_n_speed[i2cp]),
      };
      i2c_param_config(((i2cp == 0) ? I2C_NUM_0 : I2C_NUM_1), &i2cpnconf);
      if (i2c_driver_install(((i2cp == 0) ? I2C_NUM_0 : I2C_NUM_1),
                             i2cpnconf.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGI("fet-i2c.c", "I2C-Init for Port %d on SCL=GPIO%u SDA=GPIO%u failed.",
                              i2cp,
                              (settings.i2c_n_scl[i2cp] - 1),
                              (settings.i2c_n_sda[i2cp] - 1));
      } else {
        ESP_LOGI("fet-i2c.c", "I2C master port %d initialized on SCL=GPIO%u SDA=GPIO%u",
                              i2cp,
                              (settings.i2c_n_scl[i2cp] - 1),
                              (settings.i2c_n_sda[i2cp] - 1));
      }
    } else {
      ESP_LOGI("fet-i2c.c", "I2C master port %d is disabled by config", i2cp);
    }
  }
}

