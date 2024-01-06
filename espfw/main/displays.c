
/* display related functions - from initializing display hardware,
 * to drawing on images. */

#include <esp_log.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "displays.h"
#include "ssd1306.h"
#include "sdkconfig.h"
#include "settings.h"

#define TAG "displays.c"

/* Initialize the configured display (if any) */
void di_init(void)
{
    /* For now this is all hardcoded because we only support exactly
     * one type of display - FIXME */
    ssd1306_init();
}

/* Initialize a new display buffer for drawing onto. */
struct di_dispbuf * di_newdispbuf(void)
{
    int sizex = 128; int sizey = 64;
    struct di_dispbuf * res;
    res = calloc(1, sizeof(struct di_dispbuf));
    if (res == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for displaybuf (1). This will fail horribly.");
      return res;
    }
    res->cont = calloc(1, (sizex * sizey * 1));
    if (res->cont == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for displaybuf (2). This will fail horribly.");
      free(res);
      return NULL;
    }
    res->bpp = 1;
    res->sizex = sizex;
    res->sizey = sizey;
    return res;
}

/* Free a previously allocated display buffer */
void di_freedispbuf(struct di_dispbuf * db)
{
    if (db == NULL) {
      return;
    }
    if (db->cont != NULL) {
      free(db->cont);
    }
    free(db);
}

/* send display buffer to the configured display (if any) */
void di_display(struct di_dispbuf * db)
{
    ssd1306_display(db);
}

/* set a pixel */
void di_setpixel(struct di_dispbuf * db, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if ((x < 0) || (y < 0)) return;
    if ((x >= db->sizex) || (y >= db->sizey)) return;
    if (db->bpp == 1) {
      int offset = (y * db->sizex) + x;
      uint8_t v = ((r >= 0x80) ? 0xff : 0x00)
                | ((g >= 0x80) ? 0xff : 0x00)
                | ((b >= 0x80) ? 0xff : 0x00);
      db->cont[offset] = v;
    } else if (db->bpp == 3) {
      ESP_LOGW(TAG, "unimplemented pixel format - %u bpp is unsupported.", db->bpp);
    } else {
      /* Invalid pixel format. */
      ESP_LOGW(TAG, "invalid pixel format - %u bpp is unsupported.", db->bpp);
    }
}

/* get a pixel */
struct di_rgb di_getpixelrgb(struct di_dispbuf * db, int x, int y)
{
    struct di_rgb res = { .r = 0, .g = 0, .b = 0 };
    if ((x < 0) || (y < 0)) return res;
    if ((x >= db->sizex) || (y >= db->sizey)) return res;
    if (db->bpp == 1) {
      int offset = (y * db->sizex) + x;
      uint8_t v = db->cont[offset];
      res.r = res.g = res.b = v;
      return res;
    } else if (db->bpp == 3) {
      ESP_LOGW(TAG, "unimplemented pixel format - %u bpp is unsupported.", db->bpp);
      return res;
    } else {
      /* Invalid pixel format. */
      ESP_LOGW(TAG, "invalid pixel format - %u bpp is unsupported.", db->bpp);
      return res;
    }
}

uint8_t di_getpixelbw(struct di_dispbuf * db, int x, int y)
{
    if ((x < 0) || (y < 0)) return 0;
    if ((x >= db->sizex) || (y >= db->sizey)) return 0;
    if (db->bpp == 1) {
      int offset = (y * db->sizex) + x;
      return db->cont[offset];
    } else if (db->bpp == 3) {
      return 0;
    } else {
      /* Invalid pixel format. */
      ESP_LOGW(TAG, "invalid pixel format - %u bpp is unsupported.", db->bpp);
      return 0;
    }
}

static void swapint(int * a, int * b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void di_drawrect(struct di_dispbuf * db, int x1, int y1, int x2, int y2,
                 int borderwidth, uint8_t r, uint8_t g, uint8_t b)
{
    if (x1 > x2) { swapint(&x1, &x2); }
    if (y1 > y2) { swapint(&y1, &y2); }
    if (borderwidth <= 0) { /* fully filled rect */
      for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
          di_setpixel(db, x, y, r, g, b);
        }
      }
    } else {
      if ((borderwidth > (x2 - x1)) || (borderwidth > (y2 - y1))) {
        /* borderwidth is larger than the distance between our borders, this
         * will thus create a fully filled rectangle. If we tried to draw
         * that with our algorithm below however we would overflow our borders.
         * So instead call ourselves with a borderwidth of -1. */
        di_drawrect(db, x1, y1, x2, y2, -1, r, g, b);
        return;
      }
      for (int bd = 0; bd < borderwidth; bd++) {
        for (int y = y1; y <= y2; y++) {
          di_setpixel(db, x1 + bd, y, r, g, b);
          di_setpixel(db, x2 - bd, y, r, g, b);
        }
        for (int x = x1; x <= x2; x++) {
          di_setpixel(db, x, y1 + bd, r, g, b);
          di_setpixel(db, x, y2 - bd, r, g, b);
        }
      }
    }
}

void di_drawchar(struct di_dispbuf * db,
                 int x, int y, struct font * fo,
                 uint8_t r, uint8_t g, uint8_t b,
                 uint8_t c)
{
    //ESP_LOGI(TAG, "Drawing '%c' from offset %u at %d/%d", c, fo->offsets[c], x, y);
    if (fo->offsets[c] == 0) { /* This char is not in our font. */
      return;
    }
    int bpc = 1;
    if (fo->width > 16) {
      bpc = 3;
    } else if (fo->width > 8) {
      bpc = 2;
    }
    const uint8_t * fdp = fo->data + (fo->height
                                      * (fo->offsets[c] - 1)
                                      * bpc);
    for (int yo = 0; yo < fo->height; yo++) {
      uint32_t rd = *fdp;
      if (bpc >= 3) { /* 3 instead of 1 bytes per row */
        fdp++;
        rd = (rd << 8) | *fdp;
      }
      if (bpc >= 2) { /* 2 instead of 1 bytes per row */
        fdp++;
        rd = (rd << 8) | *fdp;
      }
      fdp++;
      rd = rd >> ((bpc * 8) - fo->width); /* right-align the bitmask */
      for (int xo = (fo->width - 1); xo >= 0; xo--) {
        if (rd & 1) {
          di_setpixel(db, x + xo, y + yo, r, g, b);
        }
        rd >>= 1;
      }
    }
}

void di_drawtext(struct di_dispbuf * db,
                 int x, int y, struct font * fo,
                 uint8_t r, uint8_t g, uint8_t b,
                 uint8_t * txt)
{
    while (*txt != 0) {
      //ESP_LOGI(TAG, "Drawing character '%c' at %d/%d", *txt, x, y);
      di_drawchar(db, x, y, fo, r, g, b, *txt);
      txt++;
      x += fo->width;
    }
}

int di_calctextcenter(struct font * fo, int x1, int x2, uint8_t * txt)
{
    return x1 + ((x2 - x1 - ((int)strlen(txt) * (int)fo->width)) / 2);
}
