
#ifndef _DEFWIFIAPPW_H_
#define _DEFWIFIAPPW_H_

/* This sets a password for the WiFi access point that
 * the firmware will open up for initial configuration.
 * You should absolutely set that, so you do not need to
 * do first-time configuration over unencrypted WiFi
 * with HTTP, thereby leaking secrets to anyone in WiFi
 * range.
 * so that you don't need to do first-time configuration
 * Please note however that this setting will ONLY be
 * used if you have never changed ANY setting on the
 * device, i.e. there is no config section in the flash
 * at all.
 * If you comment out the define, the accesspoint will
 * be 'open'.
 */

#define DEFAULT_WIFI_AP_PW "secret"

#endif /* _DEFWIFIAPPW_H_ */

