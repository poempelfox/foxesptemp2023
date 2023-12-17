
#include <esp_err.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "secrets.h"

struct globalsettings settings;

static void loadu8(nvs_handle_t nvshandle, const char * key, uint8_t * out)
{
  esp_err_t e = nvs_get_u8(nvshandle, key, out);
  if ((e != ESP_OK) && (e != ESP_ERR_NVS_NOT_FOUND)) {
    ESP_LOGW("settings.c", "failed to load u8 setting %s: %s", key, esp_err_to_name(e));
  }
}

static void loadstr(nvs_handle_t nvshandle, const char * key, char * out, size_t len)
{
  size_t l = len;
  esp_err_t e = nvs_get_str(nvshandle, key, out, &l);
  if ((e != ESP_OK) && (e != ESP_ERR_NVS_NOT_FOUND)) {
    ESP_LOGW("settings.c", "failed to load str setting %s: %s", key, esp_err_to_name(e));
  }
}

void settings_load(void)
{
  /* nvs_flash_init() is already done in main before we run. */
  /* bare minimum fallback settings so we have a way to configure this thing */
  uint8_t mainmac[6];
  if (esp_efuse_mac_get_custom(mainmac) != ESP_OK) {
    /* No custom MAC in EFUSE3, use default MAC */
    esp_read_mac(mainmac, ESP_MAC_WIFI_STA);
  }
  memset(&settings, 0x00, sizeof(settings));
  sprintf(settings.wifi_ap_ssid, "foxtemp%02x%02x%02x%02x%02x%02x",
          mainmac[0], mainmac[1], mainmac[2],
          mainmac[3], mainmac[4], mainmac[5]);
  strcpy(settings.adminpw, "admin");
#ifdef DEFAULT_WIFI_AP_PW
  strcpy(settings.wifi_ap_pw, DEFAULT_WIFI_AP_PW);
#endif /* DEFAULT_WIFI_AP_PW */
  nvs_handle_t nvshandle;
  if (nvs_open("settings", NVS_READONLY, &nvshandle) != ESP_OK) {
    ESP_LOGE("settings.c", "Failed to read setting from flash. Using defaults.");
    return;
  }
  loadu8(nvshandle, "wifi_mode", &(settings.wifi_mode));
  loadstr(nvshandle, "wifi_cl_ssid", settings.wifi_cl_ssid, sizeof(settings.wifi_cl_ssid));
  loadstr(nvshandle, "wifi_cl_pw", settings.wifi_cl_pw, sizeof(settings.wifi_cl_pw));
  loadstr(nvshandle, "wifi_ap_ssid", settings.wifi_ap_ssid, sizeof(settings.wifi_ap_ssid));
  loadstr(nvshandle, "wifi_ap_pw", settings.wifi_ap_pw, sizeof(settings.wifi_ap_pw));
  loadstr(nvshandle, "adminpw", settings.adminpw, sizeof(settings.adminpw));
  nvs_close(nvshandle);
}

