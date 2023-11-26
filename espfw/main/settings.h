
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
};

extern struct globalsettings settings;

/* Load main settings */
void settings_load(void);

#endif /* _SETTINGS_H_ */

