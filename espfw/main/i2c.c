
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include "settings.h"

i2c_master_bus_handle_t i2c_bushandles[2];

uint32_t i2c_settingtoi2cclock(uint8_t s)
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
      i2c_master_bus_config_t i2cpnconf = {
        .i2c_port = i2cp,
        .sda_io_num = (settings.i2c_n_sda[i2cp] - 1),
        .scl_io_num = (settings.i2c_n_scl[i2cp] - 1),
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = ((settings.i2c_n_pullups[i2cp])
                          ? true
                          : false),
      };
      if (i2c_new_master_bus(&i2cpnconf, &i2c_bushandles[i2cp]) != ESP_OK) {
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

