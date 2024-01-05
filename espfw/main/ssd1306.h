
/* Talking to SSD1306 based displays */

#ifndef _SSD1306_H_
#define _SSD1306_H_

#include "displays.h"

/* Initialize the SSD1306 based display module */
void ssd1306_init(void);

void ssd1306_display(struct di_dispbuf * db);

#endif /* _SSD1306_H_ */

