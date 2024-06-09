/* ESP32 based Foxtemp, generic edition.
 */
#include "sdkconfig.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <soc/i2c_reg.h>
#include <time.h>
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <esp_ota_ops.h>
#include <math.h>
#include <esp_netif.h>
#include "displays.h"
#include "i2c.h"
#include "lps35hw.h"
#include "network.h"
#include "rg15.h"
#include "scd41.h"
#include "sen50.h"
#include "settings.h"
#include "sgp40.h"
#include "sht4x.h"
#include "submit.h"
#include "webserver.h"

#define sleep_ms(x) vTaskDelay(pdMS_TO_TICKS(x))

static const char *TAG = "foxesptemp";

/* Global / Exported variables, used to provide the webserver.
 * struct ev is defined in webserver.h for practical reasons. */
struct ev evs[2] = { [0 ... 1] = {
                     .hum = NAN, .press = NAN, .raing = NAN, .temp = NAN,
                     .pm010 = NAN, .pm025 = NAN, .pm040 = NAN, .pm100 = NAN,
                     .co2 = 0xffff
                   } };
int activeevs = 0;
/* Has the firmware been marked as "good" yet, or is ist still pending
 * verification? */
int pendingfwverify = 0;
/* How often has the measured humidity been "too high"? */
long too_wet_ctr = 0;
/* And how many percent are "too high"? We use 90% because Sensiron uses
 * that for "way too high" in its documentation, but it might be an idea to
 * lower this to e.g. 80%. */
#define TOOWETTHRESHOLD 90.0
int forcesht4xheater = 0;
/* If we turn on the heater, how many times in a row do we do it?
 * We should do it a few times in short succession, to generate a large
 * temperature delta, that is way better for removing creep than repeating
 * a lower temperature delta more often. */
#define HEATERITS 3

struct di_dispbuf * db; /* Our main display buffer (we currently only use one) */
#define PAGE_TEMP  0
#define PAGE_HUM   1
#define PAGE_PRESS 2
#define PAGE_CO2   3
#define PAGE_PM010 4
#define PAGE_PM025 5
#define PAGE_PM040 6
#define PAGE_PM100 7
#define MAXDISPPAGES 8
uint32_t ispageenabled = 0;

/* we need this to display our IP, it is in network.c */
extern esp_netif_t * mainnetif;

void dodisplayupdate(void)
{
    static int curdisppage = -2;
    static uint8_t invertcounter = 0;
    /* First clear the whole display */
    di_drawrect(db, 0, 0, db->sizex - 1, db->sizey - 1, -1, 0x00, 0x00, 0x00);
    if (curdisppage == -100) {
      di_drawtext(db, 0,  0, &font_terminus16bold, 0xff, 0xff, 0xff, "ERROR:");
      di_drawtext(db, 0, 16, &font_terminus13norm, 0xff, 0xff, 0xff, "No sensors seem to");
      di_drawtext(db, 0, 32, &font_terminus13norm, 0xff, 0xff, 0xff, "be enabled at all,");
      di_drawtext(db, 0, 48, &font_terminus13norm, 0xff, 0xff, 0xff, "nothing to show.");
    } else if (curdisppage == -2) { /* The first startup message */
      di_drawtext(db, 0,  0, &font_terminus13norm, 0xff, 0xff, 0xff, "Hi! I'm a display!");
      di_drawtext(db, 0, 16, &font_terminus13norm, 0xff, 0xff, 0xff, "I will display mea-");
      di_drawtext(db, 0, 32, &font_terminus13norm, 0xff, 0xff, 0xff, "surements as soon");
      di_drawtext(db, 0, 48, &font_terminus13norm, 0xff, 0xff, 0xff, "as I have some.");
    } else if (curdisppage == -1) { /* Show Firmware Ver and IP, fill ispageenabled array */
      di_drawtext(db, 0,  0, &font_terminus13norm, 0xff, 0xff, 0xff, "~~~ FoxESPTemp ~~~");
      di_drawtext(db, 0, 13, &font_terminus13norm, 0xff, 0xff, 0xff, "Firmware compiled");
      const esp_app_desc_t * appd = esp_app_get_description();
      di_drawtext(db, 0, 26, &font_terminus13norm, 0xff, 0xff, 0xff, appd->date);
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(mainnetif, &ip_info) == ESP_OK) {
        uint8_t ippfb[20];
        sprintf(ippfb, IPSTR, IP2STR(&ip_info.ip));
        di_drawtext(db, 0, 39, &font_terminus13norm, 0xff, 0xff, 0xff, ippfb);
      } else {
        di_drawtext(db, 0, 39, &font_terminus13norm, 0xff, 0xff, 0xff, "No IPv4 address");
      }
      di_drawtext(db, 0, 52, &font_terminus13norm, 0xff, 0xff, 0xff, "ABCabc.,_!0123456789");
      /* Fill the ispageenabled array depending on enabled sensors */
      if (settings.sht4x_i2cport > 0) {
        ispageenabled |= 1 << PAGE_TEMP;
        ispageenabled |= 1 << PAGE_HUM;
      }
      if (settings.lps35hw_i2cport > 0) { ispageenabled |= 1 << PAGE_PRESS; }
      if (settings.scd41_i2cport > 0) { ispageenabled |= 1 << PAGE_CO2; }
      if (settings.sen50_i2cport > 0) {
        /* we display only 2 of the 4 values we have, the others don't add much
         * in terms of information but use way too much screen time and space. */
        ispageenabled |= 1 << PAGE_PM010;
        ispageenabled |= 1 << PAGE_PM100;
      }
    } else { /* curdisppage >= 0 - show values. */
      uint8_t label[30]; uint8_t value[20]; uint8_t unit[20];
      label[0] = 0; value[0] = 0; unit[0] = 0;
      if (curdisppage == PAGE_TEMP) { /* Show temp */
        strcpy(label, "Temperatur"); // we might want to translate this.
        if (isnan(evs[activeevs].temp)) {
          strcpy(value, "-.--");
        } else {
          sprintf(value, "%.2f", evs[activeevs].temp);
        }
        sprintf(unit, "%cC", 176);
      } else if (curdisppage == PAGE_HUM) { /* Show humidity */
        strcpy(label, "Luftfeuchtigkeit");
        if (isnan(evs[activeevs].hum)) {
          strcpy(value, "-.--");
        } else {
          sprintf(value, "%.2f", evs[activeevs].hum);
        }
        strcpy(unit, "%");
      } else if (curdisppage == PAGE_PRESS) { /* Show pressure */
        strcpy(label, "Luftdruck");
        if (isnan(evs[activeevs].press)) {
          strcpy(value, "---.--");
        } else {
          sprintf(value, "%.2f", evs[activeevs].press);
        }
        strcpy(unit, "hPa");
      } else if (curdisppage == PAGE_CO2) { /* Show CO2 */
        sprintf(label, "CO%c", 178);
        if (evs[activeevs].co2 == 0xffff) { /* Invalid */
          strcpy(value, "----");
        } else {
          sprintf(value, "%u", evs[activeevs].co2);
        }
        strcpy(unit, "ppm");
      } else if ((curdisppage == PAGE_PM010)
              || (curdisppage == PAGE_PM025)
              || (curdisppage == PAGE_PM040)
              || (curdisppage == PAGE_PM100)) { /* Show particulate matter */
        strcpy(label, "Feinstaub PM 1.0");
        float fv = evs[activeevs].pm010;
        if (curdisppage == PAGE_PM025) {
          strcpy(label, "Feinstaub PM 2.5");
          fv = evs[activeevs].pm025;
        } else if (curdisppage == PAGE_PM040) {
          strcpy(label, "Feinstaub PM 4.0");
          fv = evs[activeevs].pm040;
        } else if (curdisppage == PAGE_PM100) {
          strcpy(label, "Feinstaub PM 10");
          fv = evs[activeevs].pm100;
        }
        if (isnan(fv)) {
          strcpy(value, "--.-");
        } else {
          sprintf(value, "%.1f", fv);
        }
        sprintf(unit, "%cg/m%c", 181, 179);
      }
      /* Center the label */
      int xpos = di_calctextcenter(&font_terminus16bold, 0, db->sizex - 1, label);
      di_drawtext(db, xpos, 0, &font_terminus16bold, 0xff, 0xff, 0xff, label);
      /* With the values + unit, it's a bit more complicated, because they
       * are using different font sizes. */
      int vwi = strlen(value) * font_terminus38bold.width
              + strlen(unit) * font_terminus16bold.width;
      xpos = ((int)db->sizex - vwi) / 2;
      if (xpos < 0) { xpos = 0; }
      di_drawtext(db, xpos, 20, &font_terminus38bold, 0xff, 0xff, 0xff, value);
      xpos += strlen(value) * font_terminus38bold.width;
      di_drawtext(db, xpos, 20, &font_terminus16bold, 0xff, 0xff, 0xff, unit);
    }
    /* We're switching between displaying all pixels as normal and as inverted
     * to make sure all the pixels in our OLED are "on" for approximately the
     * same amount of time, as otherwise they would have very different
     * brightness levels as the display ages. */
    if (invertcounter >= 0x80) {
      di_invertall(db);
    }
    invertcounter++;
    di_display(db);
    int numcycles = 0;
    do {
      if (curdisppage > -100) { // <= -100 are error pages that should display permanently.
        curdisppage++;
      }
      if (curdisppage < 0) break; // so we don't evaluate the while condition.
      if (curdisppage >= MAXDISPPAGES) {
        curdisppage = 0;
        numcycles++;
        if (numcycles > 1) { // Ouch, we found nothing at all to display.
          curdisppage = -100; // Switch to error message page.
          break;
        }
      }
    } while ((ispageenabled & (1 << curdisppage)) == 0);
}

void app_main(void)
{
    /* This is in all OTA-Update examples, so I consider it mandatory. */
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    /* Load settings from NVRAM */
    settings_hardcode(); /* smuggles in hardcoded settings for testing */
    settings_load();

    /* Already try to start up the network (will happen mostly in the BG) */
    network_prepare();
    network_on(); /* We don't do network_off, we just try to stay connected */

    /* prepare for submitting/pushing measurements to the internet */
    submit_init();

    /* Configure our 2 I2C-ports, and then the sensors connected there. */
    i2c_port_init();
    sht4x_init();
    lps35hw_init();
    rg15_init();
    scd41_init();
    scd41_startmeas();
    sen50_init();
    sen50_startmeas(); /* FIXME Perhaps we don't want this on all the time. */
    sgp40_init();
    di_init();  /* Initialize display */
    db = di_newdispbuf();
    dodisplayupdate();
    vTaskDelay(pdMS_TO_TICKS(3000)); /* Mainly to give the RG15 a chance to */
    /* process our initialization sequence, though that doesn't always work. */

    /* Unfortunately, time does not (always) revert to 0 on an
     * esp_restart. So we set all timestamps to "now" instead. */
    time_t lastmeasts = time(NULL);
    time_t lastsht4xheat = lastmeasts;
    time_t lastsuccsubmit = lastmeasts;
    time_t lastdispupd = lastmeasts;

    /* We do NTP to provide useful timestamps in our webserver output. */
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp2.fau.de");
    esp_sntp_setservername(1, "ntp3.fau.de");
    esp_sntp_init();

    /* In case we were OTA-updating, we set this fact in a variable for the
     * webserver. Someone will need to click "Keep this firmware" in the
     * webinterface to mark the updated firmware image as good, otherwise we
     * will roll back to the previous and known working version on the next
     * reset. */
    const esp_partition_t * running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        pendingfwverify = 1;
      }
    }

    /* Start up the webserver */
    webserver_start();

    /* Wait for up to 7 more seconds to connect to WiFi and get an IP */
    EventBits_t eb = xEventGroupWaitBits(network_event_group,
                                         NETWORK_CONNECTED_BIT,
                                         pdFALSE, pdFALSE,
                                         (7000 / portTICK_PERIOD_MS));
    if ((eb & NETWORK_CONNECTED_BIT) == NETWORK_CONNECTED_BIT) {
      ESP_LOGI("main.c", "Successfully connected to network.");
    } else {
      ESP_LOGW("main.c", "Warning: Could not connect to WiFi. This is probably not good.");
    }

    /* Now loop, polling sensors and sending data once per minute. */
    while (1) {
      if (lastmeasts > time(NULL)) { /* This should not be possible, but we've
         * seen time(NULL) temporarily report insane timestamps far in the
         * future - not sure why, possibly after a watchdog reset, but in any
         * case, lets try to work around it: */
        ESP_LOGE(TAG, "Lets do the time warp again: Time jumped backwards - trying to cope.");
        lastmeasts = time(NULL); // We should return to normal measurements in 60s
        lastsht4xheat = lastmeasts;   // also reset all the other timestamps.
        lastsuccsubmit = lastmeasts;
        lastdispupd = lastmeasts;
      }
      time_t curts = time(NULL);
      if ((curts - lastmeasts) >= 60) {
        lastmeasts = curts;
        ESP_LOGI("main.c", "Telling sensors to sense...");
        /* Request update from the sensors that don't autoupdate all the time */
        lps35hw_startmeas();
        rg15_requestread();
        sht4x_startmeas();
        sgp40_startmeasraw(25.0, 50.0);
        sleep_ms(1111); /* Slightly more than a second should be enough for all sensors */
        ESP_LOGI("main.c", "Reading sensors...");
        double press = lps35hw_readpressure();
        float raing = rg15_readraincount();
        struct sht4xdata temphum;
        sht4x_read(&temphum);
        struct scd41data co2data;
        scd41_read(&co2data);
        struct sen50data pmdata;
        sen50_read(&pmdata);
        struct sgp40data vocdata;
        sgp40_read(&vocdata);

        int naevs = (activeevs == 0) ? 1 : 0;
        evs[naevs].lastupd = lastmeasts;
        /* The following is before we potentially turn on the heater and update
         * lastsht4xheat on purpose: We will only turn on the heater AFTER the
         * measurements, so it cannot affect that measurement, only the next
         * one. And the whole point of that timestamp is to allow users to
         * see whether a heating might have influenced the measurements. */
        evs[naevs].lastsht4xheat = lastsht4xheat;

        submit_clearqueue(); /* clear the queue before we queue up new values */

        if (press > 0) {
          ESP_LOGI(TAG, "Measured pressure: %.3f hPa", press);
          /* submit that measurement */
          evs[naevs].press = press;
          submit_queuevalue(ST_PRESSURE, press, 100);
        } else {
          evs[naevs].press = NAN;
        }

        if (raing > -0.1) {
          ESP_LOGI(TAG, "Rain: %.3f mm", raing);
          submit_queuevalue(ST_RAINGAUGE, raing, 100);
          evs[naevs].raing = raing;
        } else {
          evs[naevs].raing = NAN;
        }

        if (temphum.valid > 0) {
          ESP_LOGI(TAG, "Temperature: %.2f degC (raw: %x)", temphum.temp, temphum.tempraw);
          ESP_LOGI(TAG, "Humidity: %.2f %% (raw: %x)", temphum.hum, temphum.humraw);
          if (temphum.hum >= TOOWETTHRESHOLD) { /* This will cause creep */
            too_wet_ctr++;
          }
          /* creep mitigation through the integrated heater in the SHT4x.
           * This is inside temphum.valid on purpose: If we cannot communicate
           * with the sensor to read it, we probably cannot tell it to heat
           * either... */
          if (((too_wet_ctr > 60)
            && (temphum.temp >= 4.0) && (temphum.temp <= 60.0)
            && (temphum.hum <= 75.0)
            && ((time(NULL) - lastsht4xheat) > 10))
           || (forcesht4xheater > 0)) {
            /* It has been very wet for a long time, temperature is suitable
             * for heating, and heater has not been on in last 10 minutes.
             * Or someone clicked on 'force heater on' in the Web-Interface. */
            for (int i = 0; i < HEATERITS; i++) {
              if (i != 0) { vTaskDelay(pdMS_TO_TICKS(1500)); }
              sht4x_heatercycle();
            }
            lastsht4xheat = time(NULL);
            too_wet_ctr -= 30;
            forcesht4xheater = 0;
          }
          evs[naevs].temp = temphum.temp;
          evs[naevs].hum = temphum.hum;
          submit_queuevalue(ST_TEMPERATURE, temphum.temp, 100);
          submit_queuevalue(ST_HUMIDITY, temphum.hum, 100);
        } else {
          evs[naevs].temp = NAN;
          evs[naevs].hum = NAN;
        }

        if (co2data.valid > 0) {
          ESP_LOGI(TAG, "CO2: %u, LowQuality Temp: %.2f degC (raw: %x),"
                        " LQ Hum: %.2f %% (raw: %x)",
                        co2data.co2,
                        co2data.temp, co2data.tempraw,
                        co2data.hum, co2data.humraw);
          evs[naevs].co2 = co2data.co2;
          submit_queuevalue(ST_CO2, co2data.co2, 100);
        } else {
          evs[naevs].co2 = 0xffff; /* no such thing as a NAN here :-/ */
        }

        if (pmdata.valid > 0) {
          ESP_LOGI(TAG, "PM 1.0: %.1f (raw: %x)", pmdata.pm010, pmdata.pm010raw);
          ESP_LOGI(TAG, "PM 2.5: %.1f (raw: %x)", pmdata.pm025, pmdata.pm025raw);
          ESP_LOGI(TAG, "PM 4.0: %.1f (raw: %x)", pmdata.pm040, pmdata.pm040raw);
          ESP_LOGI(TAG, "PM10.0: %.1f (raw: %x)", pmdata.pm100, pmdata.pm100raw);
          evs[naevs].pm010 = pmdata.pm010;
          evs[naevs].pm025 = pmdata.pm025;
          evs[naevs].pm040 = pmdata.pm040;
          evs[naevs].pm100 = pmdata.pm100;
          submit_queuevalue(ST_PM010, pmdata.pm010, 100);
          submit_queuevalue(ST_PM025, pmdata.pm025, 100);
          submit_queuevalue(ST_PM040, pmdata.pm040, 100);
          submit_queuevalue(ST_PM100, pmdata.pm100, 100);
        } else {
          evs[naevs].pm010 = NAN;
          evs[naevs].pm025 = NAN;
          evs[naevs].pm040 = NAN;
          evs[naevs].pm100 = NAN;
        }

        if (vocdata.valid > 0) {
          ESP_LOGI(TAG, "VOC: raw %x", vocdata.vocraw);
        }

        /* Now mark the updated values as the current ones for the webserver */
        activeevs = naevs;

        /* submit values (if any). Record if we succeeded doing so. */
        ESP_LOGI(TAG, "submitting values to wetter.poempelfox.de...");
        if (submit_to_wpd() == 0) {
          lastsuccsubmit = time(NULL);
        } else {
          ESP_LOGW(TAG, "failed to submit values!");
        }
        if ((curts > 900)
         && ((curts - lastsuccsubmit) > 900)
         && ((curts - lastsuccsubmit) < 100000)) {
          /* We have been up for at least 15 minutes and not
           * successfully submitted any values in more than
           * 15 minutes (but the timespan isn't ultralong either
           * which happens when NTP jumps our time).
           * It might be a good idea to reboot. */
          ESP_LOGE(TAG, "No successful submit in %lld seconds - about to reboot.", (time(NULL) - lastsuccsubmit));
          esp_restart();
        }
      } else if ((curts - lastdispupd) >= 10) {
        lastdispupd = curts;
        /* Update display */
        dodisplayupdate();
      } else { /* Nothing to do, go back to sleep for a second. */
        /* We sadly cannot do meaningful powersaving here if we want the
         * webinterface to be reachable. */
        sleep_ms(1000);
      }
    }
}
