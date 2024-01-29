/* Functions for submitting measurements to various APIs/Websites. */

#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include "submit.h"
#include "sdkconfig.h"
#include "settings.h"

/* One entry in our queue */
struct qe {
  enum sensortypes st;
  float value;
  uint8_t prio;
};

static struct qe * theq = NULL;
static int queuesize = 0;
static int ninqueue = 0;

/* Initializes internal structure. Call once at start of program
 * and before calling anything else. */
void submit_init(void)
{
  queuesize = 5;
  theq = calloc(1, sizeof(struct qe) * queuesize);
  ninqueue = 0;
  if (theq == NULL) {
    ESP_LOGE("submit.c", "FATAL: No memory for queue. This will crash.");
  }
}

const uint8_t * st_to_name(enum sensortypes st)
{
  switch (st) {
  case ST_TEMPERATURE: return "temperature";
  case ST_HUMIDITY:    return "humidity";
  case ST_PRESSURE:    return "pressure";
  case ST_RAINGAUGE:   return "raingauge";
  case ST_CO2:         return "CO2";
  case ST_PM010:       return "PM1.0";
  case ST_PM025:       return "PM2.5";
  case ST_PM040:       return "PM4.0";
  case ST_PM100:       return "PM10";
  default:             return "UNKNOWN";
  };
}

/* clears/empties the submit queue. */
void submit_clearqueue(void)
{
  ninqueue = 0;
}

/* This queues one value for submission.
 * 'prio' is a priority, for cases where there might be multiple
 * sensors submitting measurements for e.g. temperature, but with
 * different quality: a value with higher prio replaces previous
 * queue-entries with lower prio values. */
void submit_queuevalue(enum sensortypes st, float value, uint8_t prio)
{
  if (st >= NR_SENSORTYPES) {
    ESP_LOGE("submit.c", "Trying to queue value for nonexistant sensortype!");
    return;
  }
  /* First, search if there is a queue slot we need
   * to overwrite due to prio. */
  for (int i = 0; i < ninqueue; i++) {
    if (theq[i].st == st) { /* found one! */
      if (theq[i].prio < prio) { /* It's lower prio, overwrite */
        theq[i].prio = prio;
        theq[i].value = value;
      }
      /* For same or higher prio, we do not overwrite, and throw
       * away the new value instead. */
      return;
    }
  }
  /* this sensortype is not in the queue yet. Create new entry. */
  if ((ninqueue + 1) >= queuesize) { /* Need to make array bigger. */
    queuesize = queuesize + 5;
    theq = realloc(theq, (sizeof(struct qe) * queuesize));
    if (theq == NULL) {
      ESP_LOGE("submit.c", "FATAL: No memory for queue. This will crash.");
    }
  }
  theq[ninqueue].st = st;
  theq[ninqueue].prio = prio;
  theq[ninqueue].value = value;
  ninqueue++;
}

int submit_to_wpd(void)
{
    int res = 0;
    if (settings.wpd_enabled == 0) {
      ESP_LOGI("submit.c", "Not sending data to wetter.poempelfox.de because it's disabled.");
      return 0; /* not an error */
    }
    if ((strcmp(settings.wpd_token, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM123456789") == 0)
     || (strcmp(settings.wpd_token, "") == 0)) {
      ESP_LOGI("submit.c", "Not sending data to wetter.poempelfox.de because no valid token has been set.");
      return 0; /* not an error */
    }
    char post_data[800];
    /* Build the contents of the HTTP POST we will
     * send to wetter.poempelfox.de */
    strcpy(post_data, "{\"software_version\":\"eltersdorftemp-0.1\",\"sensordatavalues\":[\n");
    int nvv = 0;
    for (int i = 0; i < ninqueue; i++) {
      if (nvv != 0) { strcat(post_data, ",\n"); }
      uint8_t * sensorid = settings.wpd_sensid[theq[i].st];
      if (strcmp(sensorid, "") == 0) {
        ESP_LOGI("submit.c", "Skipping sending data to wetter.poempelfox.de because there is no mapping for sensortype %s - arrayelement %d of %d.", st_to_name(theq[i].st), i, ninqueue);
        continue;
      }
      nvv++;
      sprintf(&post_data[strlen(post_data)],
              "{\"value_type\":\"%s\",\"value\":\"%.3f\"}",
              sensorid, theq[i].value);
    }
    strcat(post_data, "\n]}\n");
    if (nvv == 0) {
      ESP_LOGI("submit.c", "No valid values at all to submit to wetter.poempelfox.de. Skipping send.");
      return 1;
    }
    ESP_LOGI("submit.c", "wpd-payload: %d bytes: '%s'", strlen(post_data), post_data);
    esp_http_client_config_t httpcc = {
      .url = "https://wetter.poempelfox.de/api/pushmeasurement/",
      .crt_bundle_attach = esp_crt_bundle_attach,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 5000,
      .user_agent = "FoxESPTemp/0.2 (ESP32)"
    };
    esp_http_client_handle_t httpcl = esp_http_client_init(&httpcc);
    esp_http_client_set_header(httpcl, "Content-Type", "application/json");
    esp_http_client_set_header(httpcl, "X-Sensor", settings.wpd_token);
    esp_http_client_set_post_field(httpcl, post_data, strlen(post_data));
    esp_err_t err = esp_http_client_perform(httpcl);
    if (err == ESP_OK) {
      ESP_LOGI("submit.c", "HTTP POST Status = %d, content_length = %lld",
                            esp_http_client_get_status_code(httpcl),
                            esp_http_client_get_content_length(httpcl));
    } else {
      ESP_LOGE("submit.c", "HTTP POST request failed: %s", esp_err_to_name(err));
      res = 1;
    }
    esp_http_client_cleanup(httpcl);
    return res;
}

