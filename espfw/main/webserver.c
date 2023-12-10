
#include <esp_http_server.h>
#include <esp_log.h>
#include <time.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>
#include <esp_netif.h>
#include <esp_random.h>
#include "settings.h"
#include "webserver.h"

/* These are in foxesptemp_main.c */
extern struct ev evs[2];
extern int activeevs;
extern int pendingfwverify;
extern long too_wet_ctr;
extern int forcesht4xheater;
/* This is in network.c */
extern esp_netif_t * mainnetif;
static int settingshavechanged = 0;

/* How many admins can be logged in at the same time? */
#define MAXADMINLOGINS 3
/* And what is the maximum lifetime of an admin session, in seconds? */
#define MAXTOKENLIFETIME (30 * 60)
struct authtoken {
  time_t ts; /* When this token was handed out */
  char token[64]; /* the token. Always 63 bytes plus terminating \0 */
};
static struct authtoken liadmins[MAXADMINLOGINS];

/********************************************************
 * Embedded webpages definition. These are mostly       *
 * in external text files, to make editing them more    *
 * convenient.                                          *
 ********************************************************/

extern const uint8_t startp_p1[] asm("_binary_startpage_html_p00_start");
/* The following seems to exist in the autogenerated file, but it's
 * not documented - so lets avoid using it. */
/* extern uint16_t startp_p1_len asm("startpage_html_p00_length"); */

extern const uint8_t startp_p2[] asm("_binary_startpage_html_p01_start");

extern const uint8_t startpagejs[] asm("_binary_startpage_js_min_start");

extern const uint8_t csscss[] asm("_binary_css_css_min_start");

extern const uint8_t adminmenu_p1[] asm("_binary_adminmenu_html_p00_start");

extern const uint8_t adminmenu_p2[] asm("_binary_adminmenu_html_p01_start");

extern const uint8_t adminmenu_p3[] asm("_binary_adminmenu_html_p02_start");

static const char adminmenu_fww[] = R"EOAMFWW(
<br><b>A new firmware has been flashed,</b> and booted up (it's currently
running) - <b>but it has not been marked as &quot;good&quot; yet</b>.
Unless you mark the new firmware as &quot;good&quot;, on the next reset the old
firmware will be restored.<br>
<form action="adminaction" method="POST">
<input type="hidden" name="action" value="markfwasgood">
<input type="submit" name="su" value="Mark Firmware as Good"><br>
</form><br>
)EOAMFWW";

/********************************************************
 * End of embedded webpages definition                  *
 ********************************************************/

/* Helper functions */

/* Unescapes a x-www-form-urlencoded string.
 * Modifies the string inplace! */
void unescapeuestring(char * s) {
  char * rp = s;
  char * wp = s;
  while (*rp != 0) {
    if (strncmp(rp, "&amp;", 5) == 0) {
      *wp = '&'; rp += 5; wp += 1;
    } else if (strncmp(rp, "%26", 3) == 0) {
      *wp = '&'; rp += 3; wp += 1;
    } else if (strncmp(rp, "%3A", 3) == 0) {
      *wp = ':'; rp += 3; wp += 1;
    } else if (strncmp(rp, "%2F", 3) == 0) {
      *wp = '/'; rp += 3; wp += 1;
    } else {
      *wp = *rp; wp++; rp++;
    }
  }
  *wp = 0;
}

/* Expires auth tokens. Those that have exceeded MAXTOKENLIFETIME will be
 * removed from the list. */
void expireauthtokens() {
  for (int i = 0; i < MAXADMINLOGINS; i++) {
    if ((liadmins[i].ts + MAXTOKENLIFETIME) < time(NULL)) {
      liadmins[i].ts = 0;
      strcpy(liadmins[i].token, "");
    }
  }
}

static const char tokchars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

/* Creates a new entry in the list of logged in admins (liadmins[]).
 * If there is no space, one of the oldest entries will be overwritten, thereby
 * invalidating the session using it.
 * Returns a pointer to the generated new token - as it points to the entry in
 * liadmins[], it naturally must not be modified outside of this function. */
char * createnewauthtoken(void) {
  expireauthtokens();
  int selectedslot = 0;
  time_t mints = 0;
  for (int i = 0; i < MAXADMINLOGINS; i++) {
    if (liadmins[i].ts < mints) {
      selectedslot = i;
    }
  }
  liadmins[selectedslot].ts = time(NULL);
  for (int i = 0; i < 63; i++) {
    liadmins[selectedslot].token[i] = tokchars[esp_random() % strlen(tokchars)];
  }
  liadmins[selectedslot].token[63] = 0;
  return &(liadmins[selectedslot].token[0]);
}

/* Checks whether the HTTP request contains a valid authtoken cookie, i.e. the
 * user is a logged in admin.
 * Returns 1 if it does, or 0 if it does not.
 */
int checkauthtoken(httpd_req_t * req) {
  expireauthtokens(); // Clears expired ones, so we don't have to test for expiry below
  char tokenfromrequest[64];
  size_t tfrsize = sizeof(tokenfromrequest);
  esp_err_t e = httpd_req_get_cookie_val(req, "authtoken", tokenfromrequest, &tfrsize);
  if (e != ESP_OK) {
    if (e != ESP_ERR_NOT_FOUND) {
      ESP_LOGW("webserver.c", "WARNING: Shenanigans with authtokens, Error %s on get_cookie_val", esp_err_to_name(e));
    }
    return 0;
  }
  tokenfromrequest[63] = 0;
  if (strlen(tokenfromrequest) < 63) {
    ESP_LOGW("webserver.c", "invalid (too short) authtoken %s submitted by client.", tokenfromrequest);
    return 0;
  }
  for (int i = 0; i < MAXADMINLOGINS; i++) {
    if (strcmp(tokenfromrequest, liadmins[i].token) == 0) {
      /* This is a valid token. */
      return 1;
    }
  }
  /* Token is not in the list of valid tokens. */
  return 0;
}

/* Page handlers */

esp_err_t get_startpage_handler(httpd_req_t * req) {
  char myresponse[3000]; /* approx 1000 for the startpage and 2000 for the content we insert below. */
  char * pfp;
  int e = activeevs;
  strcpy(myresponse, startp_p1);
  pfp = myresponse + strlen(startp_p1);
  pfp += sprintf(pfp, "<table><tr><th>UpdateTS</th><td id=\"ts\">%lld</td></tr>", evs[e].lastupd);
  pfp += sprintf(pfp, "<tr><th>LastSHT4xHeaterTS</th><td id=\"lastsht4xheat\">%lld</td></tr>", evs[e].lastsht4xheat);
  pfp += sprintf(pfp, "<tr><th>Temperature (C)</th><td id=\"temp\">%.2f</td></tr>", evs[e].temp);
  pfp += sprintf(pfp, "<tr><th>Humidity (%%)</th><td id=\"hum\">%.1f</td></tr>", evs[e].hum);
  pfp += sprintf(pfp, "<tr><th>PM 1.0 (&micro;g/m&sup3;)</th><td id=\"pm010\">%.1f</td></tr>", evs[e].pm010);
  pfp += sprintf(pfp, "<tr><th>PM 2.5 (&micro;g/m&sup3;)</th><td id=\"pm025\">%.1f</td></tr>", evs[e].pm025);
  pfp += sprintf(pfp, "<tr><th>PM 4.0 (&micro;g/m&sup3;)</th><td id=\"pm040\">%.1f</td></tr>", evs[e].pm040);
  pfp += sprintf(pfp, "<tr><th>PM 10.0 (&micro;g/m&sup3;)</th><td id=\"pm100\">%.1f</td></tr>", evs[e].pm100);
  pfp += sprintf(pfp, "<tr><th>Pressure (hPa)</th><td id=\"press\">%.3f</td></tr>", evs[e].press);
  pfp += sprintf(pfp, "<tr><th>Rain (mm/min)</th><td id=\"raing\">%.2f</td></tr>", evs[e].raing);
  pfp += sprintf(pfp, "</table>");
  /* The following two lines are the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_startpage = {
  .uri      = "/",
  .method   = HTTP_GET,
  .handler  = get_startpage_handler,
  .user_ctx = NULL
};

esp_err_t get_json_handler(httpd_req_t * req) {
  char myresponse[1100];
  char * pfp;
  int e = activeevs;
  strcpy(myresponse, "");
  pfp = myresponse;
  pfp += sprintf(pfp, "{\"ts\":\"%lld\",", evs[e].lastupd);
  pfp += sprintf(pfp, "\"lastsht4xheat\":\"%lld\",", evs[e].lastsht4xheat);
  pfp += sprintf(pfp, "\"temp\":\"%.2f\",", evs[e].temp);
  pfp += sprintf(pfp, "\"hum\":\"%.1f\",", evs[e].hum);
  pfp += sprintf(pfp, "\"pm010\":\"%.1f\",", evs[e].pm010);
  pfp += sprintf(pfp, "\"pm025\":\"%.1f\",", evs[e].pm025);
  pfp += sprintf(pfp, "\"pm040\":\"%.1f\",", evs[e].pm040);
  pfp += sprintf(pfp, "\"pm100\":\"%.1f\",", evs[e].pm100);
  pfp += sprintf(pfp, "\"press\":\"%.3f\",", evs[e].press);
  pfp += sprintf(pfp, "\"raing\":\"%.2f\"}", evs[e].raing);
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_json = {
  .uri      = "/json",
  .method   = HTTP_GET,
  .handler  = get_json_handler,
  .user_ctx = NULL
};

esp_err_t get_publicdebug_handler(httpd_req_t * req) {
  char myresponse[2000];
  char * pfp;
  strcpy(myresponse, "");
  pfp = myresponse;
  pfp += sprintf(pfp, "<html><head><title>Debug info (public part)</title></head><body>");
  pfp += sprintf(pfp, "too_wet_ctr: %ld<br>", too_wet_ctr);
  esp_netif_ip_info_t ip_info;
  pfp += sprintf(pfp, "My IP addresses:<br><ul>");
  if (esp_netif_get_ip_info(mainnetif, &ip_info) == ESP_OK) {
    pfp += sprintf(pfp, "<li>IPv4: " IPSTR "/" IPSTR " GW " IPSTR "</li>",
                   IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask),
                   IP2STR(&ip_info.gw));
  } else {
    pfp += sprintf(pfp, "<li>Failed to get IPv4 address information :(</li>");
  }
  esp_ip6_addr_t v6addrs[CONFIG_LWIP_IPV6_NUM_ADDRESSES + 5];
  int nv6ips = esp_netif_get_all_ip6(mainnetif, v6addrs);
  for (int i = 0; i < nv6ips; i++) {
    pfp += sprintf(pfp, "<li>IPv6: " IPV6STR "</li>",
           IPV62STR(v6addrs[i]));
  }
  pfp += sprintf(pfp, "</ul>");
  pfp += sprintf(pfp, "Last reset reason: %d<br>", esp_reset_reason());
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_debug = {
  .uri      = "/debug",
  .method   = HTTP_GET,
  .handler  = get_publicdebug_handler,
  .user_ctx = NULL
};

esp_err_t get_publicstartpagejs_handler(httpd_req_t * req) {
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/javascript");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
  httpd_resp_send(req, startpagejs, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_startpage_js = {
  .uri      = "/startpage.js",
  .method   = HTTP_GET,
  .handler  = get_publicstartpagejs_handler,
  .user_ctx = NULL
};

esp_err_t get_publiccss_handler(httpd_req_t * req) {
  /* The following line is the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/css");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
  httpd_resp_send(req, csscss, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_css_css = {
  .uri      = "/css.css",
  .method   = HTTP_GET,
  .handler  = get_publiccss_handler,
  .user_ctx = NULL
};

esp_err_t get_adminmenu_handler(httpd_req_t * req) {
  char * myresponse; /* This is going to be too large to just put it on the stack. */
  char * pfp;
  if (checkauthtoken(req) != 1) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "Please log in first.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  myresponse = calloc(20000, 1); /* FIXME calculate a realistic size */
  if (myresponse == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Out of memory.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  strcpy(myresponse, adminmenu_p1);
  pfp = myresponse + strlen(myresponse);
  const esp_app_desc_t * appd = esp_app_get_description();
  pfp = myresponse + strlen(myresponse);
  pfp += sprintf(pfp, "%s version %s compiled %s %s",
                 appd->project_name, appd->version, appd->date, appd->time);
  strcat(myresponse, adminmenu_p2);
  if (pendingfwverify > 0) { /* notification that firmware has not been marked as good yet */
    strcat(myresponse, adminmenu_fww);
  }
  strcat(myresponse, adminmenu_p3);
  /* The following two lines are the default und thus redundant. */
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "private, max-age=29");
  httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  free(myresponse);
  return ESP_OK;
}

static httpd_uri_t uri_adminmenu = {
  .uri      = "/adminmenu.html",
  .method   = HTTP_GET,
  .handler  = get_adminmenu_handler,
  .user_ctx = NULL
};

esp_err_t post_adminlogin(httpd_req_t * req) {
  char postcontent[600];
  char tmp1[600];
  //ESP_LOGI("webserver.c", "POST request with length: %d", req->content_len);
  if (req->content_len >= sizeof(postcontent)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Sorry, your request was too large. Try another browser?", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  int ret = httpd_req_recv(req, postcontent, req->content_len);
  if (ret < req->content_len) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Your request was incompletely received.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  postcontent[req->content_len] = 0;
  ESP_LOGI("webserver.c", "Received data: '%s'", postcontent);
  if (httpd_query_key_value(postcontent, "adminpw", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No adminpw submitted.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  unescapeuestring(tmp1);
  if (strcmp(tmp1, settings.adminpw) != 0) {
    ESP_LOGI("webserver.c", "Incorrect AdminPW - UE: '%s'", tmp1);
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "Admin-Password incorrect.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  /* generate token, send cookie, redirect to adminmenu.html */
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", "adminmenu.html");
  sprintf(tmp1, "authtoken=%s; Max-Age=%d", createnewauthtoken(), MAXTOKENLIFETIME);
  httpd_resp_set_hdr(req, "Set-Cookie", tmp1);
  httpd_resp_send(req, "Redirecting...", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_adminlogin = {
  .uri      = "/adminlogin",
  .method   = HTTP_POST,
  .handler  = post_adminlogin,
  .user_ctx = NULL
};

esp_err_t post_adminaction(httpd_req_t * req) {
  char postcontent[600];
  char myresponse[1000];
  char tmp1[600];
  if (checkauthtoken(req) != 1) {
    httpd_resp_set_status(req, "403 Forbidden");
    strcpy(myresponse, "Please log in first.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  //ESP_LOGI("webserver.c", "POST request with length: %d", req->content_len);
  if (req->content_len >= sizeof(postcontent)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    strcpy(myresponse, "Sorry, your request was too large. Try a shorter update URL?");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  int ret = httpd_req_recv(req, postcontent, req->content_len);
  if (ret < req->content_len) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    strcpy(myresponse, "Your request was incompletely received.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  postcontent[req->content_len] = 0;
  ESP_LOGI("webserver.c", "Received data: '%s'", postcontent);
  if (httpd_query_key_value(postcontent, "action", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "No adminaction selected.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  if (strcmp(tmp1, "flashupdate") == 0) {
    if (httpd_query_key_value(postcontent, "updateurl", tmp1, sizeof(tmp1)) != ESP_OK) {
      httpd_resp_set_status(req, "400 Bad Request");
      strcpy(myresponse, "No updateurl submitted.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    unescapeuestring(tmp1);
    ESP_LOGI("webserver.c", "UE UpdateURL: '%s'", tmp1);
    sprintf(myresponse, "OK, will try to update from: %s'<br>", tmp1);
    esp_http_client_config_t httpccfg = {
        .url = tmp1,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    esp_https_ota_config_t otacfg = {
        .http_config = &httpccfg
    };
    ret = esp_https_ota(&otacfg);
    if (ret == ESP_OK) {
      ESP_LOGI("webserver.c", "OTA Succeed, Rebooting...");
      strcat(myresponse, "OTA Update reported success. Will reboot.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      vTaskDelay(3 * (1000 / portTICK_PERIOD_MS)); 
      esp_restart();
    } else {
      ESP_LOGE("webserver.c", "Firmware upgrade failed");
      strcat(myresponse, "OTA Update reported failure.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    /* This should not be reached. */
  } else if (strcmp(tmp1, "reboot") == 0) {
    ESP_LOGI("webserver.c", "Reboot requested by admin, Rebooting...");
    strcpy(myresponse, "OK, will reboot in 3 seconds.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(3 * (1000 / portTICK_PERIOD_MS)); 
    esp_restart();
    /* This should not be reached */
  } else if (strcmp(tmp1, "forcesht4xheater") == 0) {
    ESP_LOGI("webserver.c", "Forced SHT4x heating cycle requested by admin.");
    forcesht4xheater = 1;
    strcpy(myresponse, "OK, will do a SHT4x heating cycle after the next polling iteration.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  } else if (strcmp(tmp1, "markfwasgood") == 0) {
    if (pendingfwverify == 0) {
      httpd_resp_set_status(req, "400 Bad Request");
      strcpy(myresponse, "You're trying to mark an already marked firmware.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
      return ESP_OK;
    }
    ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
      ESP_LOGI("webserver.c", "markfirmwareasgood: Updated firmware is now marked as good.");
      strcpy(myresponse, "New firmware was successfully marked as good.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    } else {
      ESP_LOGE("webserver.c", "markfirmwareasgood: Failed to mark updated firmware as good, will rollback on next reboot.");
      strcpy(myresponse, "Failed to mark updated firmware as good, will rollback on next reboot.");
      httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    }
    pendingfwverify = 0;
  } else {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "Unknown adminaction requested.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
  }
  return ESP_OK;
}

static httpd_uri_t uri_adminaction = {
  .uri      = "/adminaction",
  .method   = HTTP_POST,
  .handler  = post_adminaction,
  .user_ctx = NULL
};

esp_err_t post_savesettings(httpd_req_t * req) {
  char postcontent[1000];
  char myresponse[1000];
  char tmp1[600];
  if (checkauthtoken(req) != 1) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "Please log in first.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  //ESP_LOGI("webserver.c", "POST request with length: %d", req->content_len);
  if (req->content_len >= sizeof(postcontent)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Sorry, your request was too large.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  int ret = httpd_req_recv(req, postcontent, req->content_len);
  if (ret < req->content_len) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Your request was incompletely received.", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  postcontent[req->content_len] = 0;
  ESP_LOGI("webserver.c", "Received data: '%s'", postcontent);
  /* FIXME
  if (httpd_query_key_value(postcontent, "action", tmp1, sizeof(tmp1)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    strcpy(myresponse, "No adminaction selected.");
    httpd_resp_send(req, myresponse, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  } */
  httpd_resp_send(req, "OK - dummy function", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static httpd_uri_t uri_savesettings = {
  .uri      = "/savesettings",
  .method   = HTTP_POST,
  .handler  = post_savesettings,
  .user_ctx = NULL
};

void webserver_start(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  /* Documentation is - as usual - a bit patchy, but I assume
   * the following drops the oldest connection if the ESP runs
   * out of connections. */
  config.lru_purge_enable = true;
  config.server_port = 80;
  config.max_uri_handlers = 16;
  /* The default is undocumented, but seems to be only 4k. */
  config.stack_size = 8192;
  ESP_LOGI("webserver.c", "Starting webserver on port %d", config.server_port);
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE("webserver.c", "Failed to start HTTP server.");
    return;
  }
  httpd_register_uri_handler(server, &uri_startpage);
  httpd_register_uri_handler(server, &uri_json);
  httpd_register_uri_handler(server, &uri_debug);
  httpd_register_uri_handler(server, &uri_startpage_js);
  httpd_register_uri_handler(server, &uri_css_css);
  httpd_register_uri_handler(server, &uri_adminlogin);
  httpd_register_uri_handler(server, &uri_adminmenu);
  httpd_register_uri_handler(server, &uri_adminaction);
  httpd_register_uri_handler(server, &uri_savesettings);
}

