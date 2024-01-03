
/* Global variables containing main settings, and
 * helper functions for loading/writing settings.
 */

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#define WIFIMODE_AP 0
#define WIFIMODE_CL 1

struct globalsettings {
	/* WiFi settings */
	uint8_t wifi_mode; /* AP or CLient */
	uint8_t wifi_cl_ssid[33];
	uint8_t wifi_cl_pw[64];
	uint8_t wifi_ap_ssid[33];
	uint8_t wifi_ap_pw[64];
	/* Password for the Admin pages in the Webinterface */
	uint8_t adminpw[25];
	/* Token for submitting values to wetter.poempelfox.de */
	uint8_t wpdtoken[65];
	/* the pins used for I2C, 0 for disable, pinnumber+1 otherwise. */
	uint8_t i2c_n_scl[2];
	uint8_t i2c_n_sda[2];
	uint8_t i2c_n_pullups[2]; // pullups enabled in the GPIO
	/* On which I2C bus are the respective sensors? Again, this
	 * is 0 to disable the sensor, or busnumber+1 otherwise. */
	uint8_t lps35hw_i2cport;
	uint8_t lps35hw_addr; // offset to 0x5c! see lps35hw.c
	uint8_t scd41_i2cport;
	uint8_t sen50_i2cport;
	uint8_t sht4x_addr; // offset to 0x44! see sht4x.c
	uint8_t sht4x_i2cport;
};

extern struct globalsettings settings;

/* Load main settings */
void settings_load(void);

void settings_hardcode(void);

#endif /* _SETTINGS_H_ */

