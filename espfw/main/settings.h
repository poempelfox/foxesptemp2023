
/* Global variables containing main settings, and
 * helper functions for loading/writing settings.
 */

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

/* we need this for NR_SENSORTYPES */
#include "submit.h"

#define WIFIMODE_AP 0
#define WIFIMODE_CL 1

/* Types of displays.
 * we need to fix the values, as they show up in settings stored in
 * flash, so they must not change as new sensor types are added. */
enum di_displaytypes {
  DI_DT_NONE = 0,
  // Winstar WEA012864DWPP3N00003, SSD1306 based, likely to work
  // for some other 128x64 displays
  DI_DT_SSD1306_1 = 1,
  // Zhongjingyuan SSD1309 based 2.42 inch 128x64 display
  DI_DT_SSD1309_1 = 2,
};

struct globalsettings {
	/* WiFi settings */
	uint8_t wifi_mode; /* AP or CLient */
	uint8_t wifi_cl_ssid[33];
	uint8_t wifi_cl_pw[64];
	uint8_t wifi_ap_ssid[33];
	uint8_t wifi_ap_pw[64];
	/* Password for the Admin pages in the Webinterface */
	uint8_t adminpw[25];
	/* the pins used for I2C, 0 for disable, GPIOnumber+1 otherwise. */
	uint8_t i2c_n_scl[2];
	uint8_t i2c_n_sda[2];
	uint8_t i2c_n_pullups[2]; // pullups enabled in the GPIO
	uint8_t i2c_n_speed[2]; // Speed
	/* the pins used for Serial UART, 0 for disable, GPIOnumber+1 otherwise. */
	uint8_t ser_1_rx;
	uint8_t ser_1_tx;
	/* On which I2C bus are the respective sensors? Again, this
	 * is 0 to disable the sensor, or busnumber+1 otherwise. */
	uint8_t lps35hw_i2cport;
	uint8_t lps35hw_addr; // offset to 0x5c! see lps35hw.c
	uint8_t scd41_i2cport;
	uint8_t scd41_selfcal;
	uint8_t sen50_i2cport;
	uint8_t sht4x_addr; // offset to 0x44! see sht4x.c
	uint8_t sht4x_i2cport;
	/* On which serial port are the respective sensors? */
	uint8_t rg15_serport;
	/* Display settings */
	uint8_t di_type; // see enum di_displaytypes
	uint8_t di_i2cport; // for I2C displays
	/* Settings for submitting values to wetter.poempelfox.de */
        uint8_t wpd_enabled;
	uint8_t wpd_token[65]; /* Token for authentication. */
	uint8_t wpd_sensid[NR_SENSORTYPES][12];
};

extern struct globalsettings settings;

/* Load main settings */
void settings_load(void);

void settings_hardcode(void);

#endif /* _SETTINGS_H_ */

