
#ifndef _SUBMIT_H_
#define _SUBMIT_H_

/* An array of the following structs is handed to the
 * submit_to_opensensemap_multi function */
struct wpds {
  const char * sensorid;
  float value;
};

/* The IDs of our sensors on wetter.poempelfox.de. */
#define WPDSID_TEMPERATURE "88"
#define WPDSID_HUMIDITY "89"
#define WPDSID_PRESSURE "90"
#define WPDSID_RAINGAUGE "91"
#define WPDSID_PM010 "92"
#define WPDSID_PM025 "93"
#define WPDSID_PM040 "94"
#define WPDSID_PM100 "95"

/* Submits multiple values to the wetter.poempelfox.de API
 * in one HTTPS request */
int submit_to_wpd_multi(int arraysize, struct wpds * arrayofwpds);

/* This is a convenience function, calling ..._wpd_multi
 * with a size 1 array internally. */
int submit_to_wpd(char * sensorid, float value);

#endif /* _SUBMIT_H_ */

