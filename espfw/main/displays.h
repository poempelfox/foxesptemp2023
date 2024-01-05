
/* display related functions - from initializing display hardware,
 * to drawing on images. */

#ifndef _DISPLAYS_H_
#define _DISPLAYS_H_

/* This is a display buffer that holds all screen output, for
 * drawing onto it and later sending it to the display hardware
 * for displaying it.
 * Note that in the buffer we always use at least 1 byte per
 * pixel - even if we have a display that only has 1 bit.
 */
struct di_dispbuf {
  uint16_t sizex;
  uint16_t sizey;
  uint8_t bpp; /* 1 or 3 or 4 - only 1 currently implemented */
  uint8_t * cont;
};

struct di_rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

/* Initialize the configured display (if any) */
void di_init(void);

/* Initialize a new display buffer for drawing onto. */
struct di_dispbuf * di_newdispbuf(void);

/* Free a previously allocated display buffer */
void di_freedispbuf(struct di_dispbuf * db);

/* send display buffer to the configured display (if any) */
void di_display(struct di_dispbuf * db);

/* set a pixel */
void di_setpixel(struct di_dispbuf * db, int x, int y, uint8_t r, uint8_t g, uint8_t b);

/* get a pixel */
struct di_rgb di_getpixelrgb(struct di_dispbuf * db, int x, int y);
uint8_t di_getpixelbw(struct di_dispbuf * db, int x, int y);

#endif /* _DISPLAYS_H_ */

