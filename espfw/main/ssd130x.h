
/* Talking to SSD130x based displays (e.g. SSD1306 and SSD1309) */

#ifndef _SSD130X_H_
#define _SSD130X_H_

#include "displays.h"

/* Initialize the SSD130x based display module */
void ssd130x_init(void);

/* Display a dispbuf on that display */
void ssd130x_display(struct di_dispbuf * db);

#endif /* _SSD130X_H_ */

