
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include "settings.h"

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
        .master.clk_speed = 100000, /* There is really no need to hurry */
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

