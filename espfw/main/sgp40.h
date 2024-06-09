
/* Talking to SGP40 VOC (volatile organic compounds) sensors */

#ifndef _SGP40_H_
#define _SGP40_H_

struct sgp40data {
  uint8_t valid;
  uint16_t vocraw;
};

/* Initialize the SGP40 */
void sgp40_init(void);

/* Request measurements from the SGP40 */
void sgp40_startmeasraw(float temp, float hum);

/* Read the raw VOC data from the sensor.
 * You need to request a measurements at least 30 ms
 * before reading, and you can only read every
 * measurement at most once! */
void sgp40_read(struct sgp40data * d);

#endif /* _SGP40_H_ */

