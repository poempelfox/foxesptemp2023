
/* display related functions - from initializing display hardware,
 * to drawing on images. */

#include <esp_log.h>
#include <stddef.h>
#include <stdlib.h>
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
    } else if (db->bpp == 4) {
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
    } else if (db->bpp == 4) {
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
    } else if (db->bpp == 4) {
      return 0;
    } else {
      /* Invalid pixel format. */
      ESP_LOGW(TAG, "invalid pixel format - %u bpp is unsupported.", db->bpp);
      return 0;
    }
}

