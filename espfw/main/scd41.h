
/* Talking to SCD41 CO2 sensors. */

#ifndef _SCD41_H_
#define _SCD41_H_

#include "driver/i2c.h" /* Needed for i2c_port_t */

struct scd41data {
  uint8_t valid;
  uint16_t co2; /* CO2 - no 'raw' because there is no conversion needed. */
  /* Note: This is mainly a CO2 sensor. It does also
   * measure temperature and rel. humidity, but it is
   * only a mediocre sensor for that. */
  uint16_t tempraw; /* temperature */
  uint16_t humraw; /* rel.hum. */
  float temp; /* temperature */
  float hum; /* rel.hum. */
};

/* Initialize the SCD41 */
void scd41_init(i2c_port_t port);

/* Start periodic measurements on the SCD41.
 * Note: We use the low power mode that provides a measurement every 30s,
 * not the normal mode that does so every 5s. */
void scd41_startmeas(void);
/* Stop measurements */
void scd41_stopmeas(void);

/* Read measurement data from the sensor.
 * This will return an error if no new data is available on this sensor type! */
void scd41_read(struct scd41data * d);

#endif /* _SCD41_H_ */

