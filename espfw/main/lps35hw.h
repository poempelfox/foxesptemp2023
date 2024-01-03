
#ifndef _LPS35HW_H_
#define _LPS35HW_H_

void lps35hw_init(void);

/* Starts a one-shot measurement. Unfortunately, it is not
 * documented how long that will take. However, since you can
 * configure the sensor to between 1 and 75 measurements per
 * second in continous mode, it should be safe to assume it
 * won't take longer than a second, probably a lot less. */
void lps35hw_startmeas(void);
/* Read the result of the previous measurement. */
double lps35hw_readpressure(void);

#endif /* _LPS35HW_H_ */

