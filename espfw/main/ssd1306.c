/* Talking to SSD1306 based displays */

#include <driver/i2c.h>
#include <esp_log.h>
#include "ssd1306.h"
#include "sdkconfig.h"
#include "settings.h"

#define SSD1306BASEADDR 0x3c /* address is 0x3c or 0x3d, depending on how pin SA0 is wired. */

#define I2C_MASTER_TIMEOUT_MS 1000  /* Timeout for I2C communication */

static i2c_port_t ssd1306i2cport;
static uint8_t ssd1306addr; /* the final calculated address, (BASEADDR + offset) */

/* A very short summary of the protocol the display uses:
 * After addressing the display, there is always a 'control' byte, which
 * mainly determines whether the following byte(s) are a 'command' or 'data',
 * and has 6 unused bits. */
#define CONTROL_CONT 0x80  /* "Continuation" bit. 0 means only data follows. */
#define CONTROL_NOCO 0x00  /* "Continuation" bit. 0 means only data follows. */
#define CONTROL_DATA 0x40  /* "command or data" - bit set means data follows, */
#define CONTROL_CMD  0x00  /*                     otherwise it is a command. */

/* Send a 1 byte command */
static void ssd1306_sendcommand1(uint8_t cmd)
{
    uint8_t tosend[2];
    tosend[0] = CONTROL_NOCO | CONTROL_CMD;
    tosend[1] = cmd;
    i2c_master_write_to_device(ssd1306i2cport, ssd1306addr, tosend, 2,
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/* Send a 2 byte command */
static void ssd1306_sendcommand2(uint8_t cmd1, uint8_t cmd2)
{
    uint8_t tosend[3];
    tosend[0] = CONTROL_NOCO | CONTROL_CMD;
    tosend[1] = cmd1;
    tosend[2] = cmd2;
    i2c_master_write_to_device(ssd1306i2cport, ssd1306addr, tosend, 3,
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/* Send a 3 byte command */
static void ssd1306_sendcommand3(uint8_t cmd1, uint8_t cmd2, uint8_t cmd3)
{
    uint8_t tosend[4];
    tosend[0] = CONTROL_NOCO | CONTROL_CMD;
    tosend[1] = cmd1;
    tosend[2] = cmd2;
    tosend[3] = cmd3;
    i2c_master_write_to_device(ssd1306i2cport, ssd1306addr, tosend, 4,
                               I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void ssd1306_init(void)
{
#if 0 /* FIXME settings not implemented yet */
    if (settings.ssd1306_i2cport > 0) {
      /* An I2C-port is configured, but is that port disabled? */
      if ((settings.i2c_n_scl[settings.ssd1306_i2cport - 1] == 0)
       || (settings.i2c_n_sda[settings.ssd1306_i2cport - 1] == 0)) {
        /* It is. Force disabled-setting for us too. */
        settings.ssd1306_i2cport = 0;
        ESP_LOGW("ssd1306.c", "WARNING: SSD1306 automatically disabled because it is connected to a disabled I2C port.");
      }
    }
    if (settings.ssd1306_i2cport == 0) return;
    ssd1306i2cport = ((settings.ssd1306_i2cport == 1) ? I2C_NUM_0 : I2C_NUM_1);
    ssd1306addr = SSD1306BASEADDR + settings.ssd1306_addr;
#else
    /* Hardcoded settings for now. */
    ssd1306i2cport = I2C_NUM_0;
    ssd1306addr = SSD1306BASEADDR + 0;
#endif

    /* The initialization sequence is essentially copy+paste from the datasheet
     * of our Winstar WEA012864DWPP3N00003, and probably needs some tweaking
     * to work with other display modules using the same chip. */
    ssd1306_sendcommand1(0xAE);        /* Display OFF */
    ssd1306_sendcommand2(0xD5, 0x80);  /* Set Display Clock to 105Hz */
    ssd1306_sendcommand2(0xA8, 0x3F);  /* Select default Multiplex ratio (MUX64) */
    ssd1306_sendcommand2(0xD3, 0x00);  /* Set display offset to 0 */
    ssd1306_sendcommand1(0x40);        /* Set display start line to 0 */
    ssd1306_sendcommand2(0x8D, 0x14);  /* Enable internal charge pump at the default 7.5V */
    ssd1306_sendcommand2(0xAD, 0x30);  /* Enable internal Iref at 30uA, resulting in a maximum Iseg=240uA */
    ssd1306_sendcommand1(0xA0);        /* Segment remap (mirroring all pixels left<->right) */
    ssd1306_sendcommand1(0xC8);        /* COM output scan direction: remapped */
    ssd1306_sendcommand2(0xDA, 0x12);  /* alternative COM pin configuration, no COM left/right remap */
    ssd1306_sendcommand2(0x81, 0xFF);  /* Set contrast to '0xff' (range 0x01-0xff, default of chip 0x7f) */
    ssd1306_sendcommand2(0xD9, 0x22);  /* Set precharge period, phase 1 = 2 DCLK, phase 2 = 2 DCLK, that is also the default of the chip. */
    ssd1306_sendcommand2(0xDB, 0x30);  /* Set Vcomh deselect level to 0.83 * Vcc */
    ssd1306_sendcommand1(0xA4);        /* Display ON, output follows RAM contents */
    ssd1306_sendcommand1(0xA6);        /* Normal non-inverted display (A7 to invert) */
    ssd1306_sendcommand1(0xAF);        /* Display ON in normal mode */
}

void ssd1306_display(struct di_dispbuf * db)
{
    /* The display has a somewhat weird memory layout: Each byte in memory
     * addresses one column for 8 rows, with the LSB being (relative) row 0 and
     * the MSB being (relative) row 7. There are 8 "pages". Each page contains
     * 128 bytes of memory, for 128 columns times 8 rows. */
    ssd1306_sendcommand2(0x20, 0x00);     /* Set horizontal addressing mode */
    ssd1306_sendcommand3(0x21,   0, 127); /* Set column start and end address */
    ssd1306_sendcommand3(0x22,   0,   7); /* Set page start and end address */
    uint8_t sndbuf[129];
    for (int page = 0; page < 8; page++) {
      sndbuf[0] = (CONTROL_DATA | CONTROL_NOCO);
      for (int col = 0; col < 128; col++) {
        int rs = page * 8;
        uint8_t b = 0;
        for (int row = 0; row < 8; row++) {
          uint8_t p = di_getpixelbw(db, col, (rs + row));
          if (p > 0x80) {
            b |= (1U << row);
          }
        }
        sndbuf[col + 1] = b;
      }
      for (int row = 0; row < 8; row++) {
        uint8_t opb[200];
        for (int col = 0; col < 128; col++) {
          if ((sndbuf[col + 1] & (1U << row)) > 0) {
            opb[col] = 'X';
          } else {
            opb[col] = ' ';
          }
        }
        opb[128] = 0;
        ESP_LOGI("debug-display", "%s", opb);
      }
      i2c_master_write_to_device(ssd1306i2cport, ssd1306addr, sndbuf, 129,
                                 I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    }
}
