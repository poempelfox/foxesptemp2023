
#ifndef _SUBMIT_H_
#define _SUBMIT_H_

/* Types of sensors.
 * we need to fix the values, as they show up in settings stored in
 * flash, so they must not change as new sensor types are added. */
enum sensortypes {
  ST_TEMPERATURE = 0,
  ST_HUMIDITY = 1,
  ST_PRESSURE = 2,
  ST_RAINGAUGE = 3,
  ST_CO2 = 4,
  ST_PM010 = 5,
  ST_PM025 = 6,
  ST_PM040 = 7,
  ST_PM100 = 8,
};

/* Initializes internal structure. Call once at start of program
 * and before calling anything else. */
void submit_init(void);

/* clears/empties the submit queue. */
void submit_clearqueue(void);

/* This queues one value for submission.
 * 'prio' is a priority, for cases where there might be multiple
 * sensors submitting measurements for e.g. temperature, but with
 * different quality: a value with higher prio replaces previous
 * queue-entries with lower prio values. */
void submit_queuevalue(enum sensortypes st, float value, uint8_t prio);

/* Submits all queued values to the wetter.poempelfox.de API
 * in one HTTPS request */
int submit_to_wpd(void);

#endif /* _SUBMIT_H_ */

