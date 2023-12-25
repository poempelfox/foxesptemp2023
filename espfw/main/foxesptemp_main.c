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
#include "i2c.h"
#include "lps35hw.h"
#include "network.h"
#include "rg15.h"
#include "scd41.h"
#include "sen50.h"
#include "settings.h"
#include "sht4x.h"
#include "submit.h"
#include "webserver.h"

#define sleep_ms(x) vTaskDelay(pdMS_TO_TICKS(x))

static const char *TAG = "foxesptemp";

/* Global / Exported variables, used to provide the webserver.
 * struct ev is defined in webserver.h for practical reasons. */
struct ev evs[2];
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


void app_main(void)
{
    memset(evs, 0, sizeof(evs));

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

    /* Configure our 2 I2C-ports, and then the sensors connected there. */
    i2c_port_init();
    sht4x_init(I2C_NUM_0);
    lps35hw_init(I2C_NUM_0);
    rg15_init();
    scd41_init(I2C_NUM_0);
    scd41_startmeas();
    sen50_init(I2C_NUM_1);
    sen50_startmeas(); /* FIXME Perhaps we don't want this on all the time. */    
    vTaskDelay(pdMS_TO_TICKS(3000)); /* Mainly to give the RG15 a chance to */
    /* process our initialization sequence, though that doesn't always work. */

    /* Unfortunately, time does not (always) revert to 0 on an
     * esp_restart. So we set all timestamps to "now" instead. */
    time_t lastmeasts = time(NULL);
    time_t lastsht4xheat = lastmeasts;
    time_t lastsuccsubmit = lastmeasts;

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
    const esp_partition_t *running = esp_ota_get_running_partition();
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
      }
      time_t curts = time(NULL);
      if ((curts - lastmeasts) >= 60) {
        lastmeasts = curts;
        ESP_LOGI("main.c", "Telling sensors to sense...");
        /* Request update from the sensors that don't autoupdate all the time */
        lps35hw_startmeas();
        rg15_requestread();
        sht4x_startmeas();
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

        int naevs = (activeevs == 0) ? 1 : 0;
        evs[naevs].lastupd = lastmeasts;
        /* The following is before we potentially turn on the heater and update
         * lastsht4xheat on purpose: We will only turn on the heater AFTER the
         * measurements, so it cannot affect that measurement, only the next
         * one. And the whole point of that timestamp is to allow users to
         * see whether a heating might have influenced the measurements. */
        evs[naevs].lastsht4xheat = lastsht4xheat;

        struct wpds tosubmit[11]; /* we'll submit at most 8 values because we have that many sensors */
        int nts = 0; /* Number of values to submit */
        /* Lets define a little helper macro to limit the copy+paste orgies */
        #define QUEUETOSUBMIT(s, v)  tosubmit[nts].sensorid = s; tosubmit[nts].value = v; nts++;

        if (press > 0) {
          ESP_LOGI(TAG, "Measured pressure: %.3f hPa", press);
          /* submit that measurement */
          evs[naevs].press = press;
          QUEUETOSUBMIT(WPDSID_PRESSURE, press);
        } else {
          evs[naevs].press = NAN;
        }

        if (raing > -0.1) {
          ESP_LOGI(TAG, "Rain: %.3f mm", raing);
          QUEUETOSUBMIT(WPDSID_RAINGAUGE, raing);
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
          QUEUETOSUBMIT(WPDSID_TEMPERATURE, temphum.temp);
          QUEUETOSUBMIT(WPDSID_HUMIDITY, temphum.hum);
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
          QUEUETOSUBMIT(WPDSID_CO2, co2data.co2);
        } else {
          evs[naevs].co2 = 0xffff; /* no such thing as a NaN here :-/ */
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
          QUEUETOSUBMIT(WPDSID_PM010, pmdata.pm010);
          QUEUETOSUBMIT(WPDSID_PM025, pmdata.pm025);
          QUEUETOSUBMIT(WPDSID_PM040, pmdata.pm040);
          QUEUETOSUBMIT(WPDSID_PM100, pmdata.pm100);
        } else {
          evs[naevs].pm010 = NAN;
          evs[naevs].pm025 = NAN;
          evs[naevs].pm040 = NAN;
          evs[naevs].pm100 = NAN;
        }
        /* Clean up helper macro */
        #undef QUEUETOSUBMIT

        /* Now mark the updated values as the current ones for the webserver */
        activeevs = naevs;

        /* submit values (if any). Record if we succeeded doing so. */
        ESP_LOGI(TAG, "have %d values to submit...", nts);
        if (nts > 0) {
          if (submit_to_wpd_multi(nts, tosubmit) == 0) {
            ESP_LOGI(TAG, "successfully submitted values.");
            lastsuccsubmit = time(NULL);
          } else {
            ESP_LOGW(TAG, "failed to submit values!");
          }
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
      } else { /* Woke up too early, go back to sleep */
        sleep_ms(1000);
      }
    }
}

