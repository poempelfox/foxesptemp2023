
#include "network.h"
#include <driver/gpio.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <nvs_flash.h>
#include <string.h>
#include <time.h>
#include "sdkconfig.h"
#include "settings.h"

EventGroupHandle_t network_event_group;
esp_netif_t * mainnetif = NULL;


/** Event handler for WiFi events */
static time_t lastwifireconnect = 0;
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_connected_t * ev_co = (wifi_event_sta_connected_t *)event_data;
    wifi_event_sta_disconnected_t * ev_dc = (wifi_event_sta_disconnected_t *)event_data;
    switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI("network.c", "WiFi Connected: channel %u bssid %02x%02x%02x%02x%02x%02x",
                           ev_co->channel, ev_co->bssid[0], ev_co->bssid[1], ev_co->bssid[2],
                           ev_co->bssid[3], ev_co->bssid[4], ev_co->bssid[5]);
            /* This not only enables the linklocal address, but it also causes
             * the ESP to accept the addresses from route advertisements. */
            esp_netif_create_ip6_linklocal(mainnetif);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI("network.c", "WiFi Disconnected: reason %u", ev_dc->reason);
            if (ev_dc->reason == WIFI_REASON_ASSOC_LEAVE) break; /* This was an explicit call to disconnect() */
            if ((lastwifireconnect == 0)
             || ((time(NULL) - lastwifireconnect) > 5)) {
              /* Last reconnect attempt more than 5 seconds ago - try it again */
              ESP_LOGI("network.c", "Attempting WiFi reconnect");
              lastwifireconnect = time(NULL);
              esp_wifi_connect();
            }
            break;
        default: break;
    }
}

/* Event handler for IP_EVENT_(ETH|STA)_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t * event4;
    const esp_netif_ip_info_t *ip_info;
    ip_event_got_ip6_t * event6;
    const esp_netif_ip6_info_t * ip6_info;
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
      ESP_LOGI("network.c", "We got an IPv4 address!");
      event4 = (ip_event_got_ip_t *)event_data;
      ip_info = &event4->ip_info;
      ESP_LOGI("network.c", "IP:     " IPSTR, IP2STR(&ip_info->ip));
      ESP_LOGI("network.c", "NETMASK:" IPSTR, IP2STR(&ip_info->netmask));
      ESP_LOGI("network.c", "GW:     " IPSTR, IP2STR(&ip_info->gw));
      break;
    case IP_EVENT_GOT_IP6:
      ESP_LOGI("network.c", "We got an IPv6 address!");
      event6 = (ip_event_got_ip6_t *)event_data;
      ip6_info = &event6->ip6_info;
      ESP_LOGI("network.c", "IPv6:" IPV6STR, IPV62STR(ip6_info->ip));
      break;
    case IP_EVENT_STA_LOST_IP:
      ESP_LOGI("network.c", "IP-address lost.");
    };
    xEventGroupSetBits(network_event_group, NETWORK_CONNECTED_BIT);
}

void network_prepare(void)
{
    /* WiFi does not work without this because... who knows, who cares. */
    nvs_flash_init();
    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that s running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    network_event_group = xEventGroupCreate();
    if (settings.wifi_mode == WIFIMODE_CL) { /* client */
      mainnetif = esp_netif_create_default_wifi_sta();
      // to avoid having to use another setting, we just reuse the AP name as the client hostname
      esp_netif_set_hostname(mainnetif, settings.wifi_ap_ssid);
      wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
      ESP_ERROR_CHECK(esp_wifi_init(&wicfg));
      ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
      // Register user defined event handers
      ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
      ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL));
      ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &got_ip_event_handler, NULL));
      ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &got_ip_event_handler, NULL));
      wifi_config_t wc = {
        .sta = {
          .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
      };
      strlcpy(wc.sta.ssid, settings.wifi_cl_ssid, sizeof(wc.sta.ssid));
      if (strlen(settings.wifi_cl_pw) > 0) {
        strlcpy(wc.sta.password, settings.wifi_cl_pw, sizeof(wc.sta.password));
      } else {
        wc.sta.threshold.authmode = WIFI_AUTH_OPEN;
      }
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wc));
      ESP_LOGI("network.c", "Will try to connect to WiFi with SSID %s and password %s",
               wc.sta.ssid, wc.sta.password);
    } else { /* we're not a client but an access point */
      wifi_config_t wc = {
        .ap.max_connection = 8,
        .ap.pmf_cfg = {
          .required = true,
        },
      };
      strlcpy(wc.ap.ssid, settings.wifi_ap_ssid, sizeof(wc.ap.ssid));
      if (strlen(settings.wifi_ap_pw) > 0) {
        wc.ap.authmode = WIFI_AUTH_WPA2_PSK;
        strlcpy(wc.ap.password, settings.wifi_ap_pw, sizeof(wc.ap.password));
      } else {
        wc.ap.authmode = WIFI_AUTH_OPEN;
      }
      mainnetif = esp_netif_create_default_wifi_ap();
      esp_netif_dhcp_status_t dhcpst;
      ESP_ERROR_CHECK(esp_netif_dhcps_get_status(mainnetif, &dhcpst));
      if (dhcpst != ESP_NETIF_DHCP_STOPPED) {
        ESP_LOGI("network.c", "Temporarily stopping DHCP server");
        /* DHCP is started, so stop it and restart after changing settings. */
        esp_netif_dhcps_stop(mainnetif);
      }
      ESP_ERROR_CHECK(esp_netif_set_hostname(mainnetif, "foxtemp"));
      esp_netif_ip_info_t ipi;
      esp_netif_set_ip4_addr(&ipi.ip,10,5,5,1);
      esp_netif_set_ip4_addr(&ipi.gw,10,5,5,1);
      esp_netif_set_ip4_addr(&ipi.netmask,255,255,255,0);
      ESP_ERROR_CHECK(esp_netif_set_ip_info(mainnetif, &ipi));
      if (dhcpst != ESP_NETIF_DHCP_STOPPED) {
        esp_netif_dhcps_start(mainnetif);
      }

      wifi_init_config_t wic = WIFI_INIT_CONFIG_DEFAULT();
      ESP_ERROR_CHECK(esp_wifi_init(&wic));
      ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
#if 0
      /* No 802.11b, only g and better.
       * Supporting 802.11b causes a MASSIVE waste of airtime, and it
       * hasn't been of any use for many years now. That is why at most
       * hacker camps, the network rules state "turn of that nonsense".
       * While we would love to do that, unfortunately Espressif is too
       * lazy to support that. It is - as of ESP-IDF 5.0.1 - not possible
       * to turn off b, the following line will just throw an "invalid
       * argument". */
      ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP, (WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N)));
#endif
      /* Only 20 MHz channel width, not 40. */
      ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20));
    } /* we're an accesspoint */
}

void network_on(void)
{
    esp_err_t e = esp_wifi_start();
    if (e != ESP_OK) {
      ESP_LOGE("network.c", "Failed to start WiFi! Error returned: %s", esp_err_to_name(e));
      return;
    }
    if (settings.wifi_mode == WIFIMODE_CL) {
      e = esp_wifi_connect();
      if (e != ESP_OK) {
        ESP_LOGE("network.c", "Failed wifi_connect! Error returned: %s", esp_err_to_name(e));
      }
    } else {
      /* We're always "connected" if we're the accesspoint */
      xEventGroupSetBits(network_event_group, NETWORK_CONNECTED_BIT);
    }
}

void network_off(void)
{
    xEventGroupClearBits(network_event_group, NETWORK_CONNECTED_BIT);
    ESP_ERROR_CHECK(esp_wifi_stop());
}

