
#include <esp_console.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include "console.h"

/* Note: REPL = Read Evaluate Print Loop */
esp_console_repl_t * repl = NULL;

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) \
  || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM) \
  || defined(CONFIG_ESP_CONSOLE_USB_CDC)

static int console_cmd_restart(int argc, char ** argv) {
  ESP_LOGI("console.c", "Executing exp_restart()...");
  esp_restart();
  /* This will not be reached */
  return 0;
}

static int console_cmd_factory_reset(int argc, char ** argv) {
  if ((argc != 2) || (strcmp(argv[1], "-f") != 0)) {
    ESP_LOGE("console.c", "You need to call factory_reset with a single argument '-f' to force erasing all configuration.");
    return 1;
  }
  ESP_LOGI("console.c", "Calling nvs_flash_erase to clear all configuration...");
  esp_err_t e = nvs_flash_erase();
  if (e == ESP_OK) {
    ESP_LOGI("console.c", "Success. You will now need to reboot the ESP, e.g. by calling the command 'restart'!");
    return 0;
  } else {
    ESP_LOGE("console.c", "nvs_flash_erase() returned an error: %s", esp_err_to_name(e));
    return 1;
  }
}

static int console_cmd_nvs_list(int argc, char ** argv) {
  nvs_iterator_t nvsit = NULL;
  ESP_LOGI("console.c", " %-20s %-3s", "settingsname", "type");
  esp_err_t e = nvs_entry_find(NVS_DEFAULT_PART_NAME, "settings", NVS_TYPE_ANY, &nvsit);
  while (e == ESP_OK) {
    nvs_entry_info_t ei;
    e = nvs_entry_info(nvsit, &ei);
    if (e != ESP_OK) {
      ESP_LOGE("console.c", "nvs_entry_info failed: %s", esp_err_to_name(e));
      break;
    }
    char et[4] = "???";
    if (ei.type == NVS_TYPE_STR) { strcpy(et, "STR"); }
    if (ei.type == NVS_TYPE_U8) { strcpy(et, "U08"); }
    ESP_LOGI("console.c", " %-20s %-3s", ei.key, et);
    e = nvs_entry_next(&nvsit);
  }
  nvs_release_iterator(nvsit);
  return 0;
}

static int console_cmd_nvs_get(int argc, char ** argv) {
  if (argc != 2) {
    ESP_LOGE("console.c", "nvs_get needs exactly one argument: the name of the setting you want to show.");
    ESP_LOGE("console.c", "Example: nvs_get wifi_cl_ssid");
    return 1;
  }
  nvs_handle_t nvshandle;
  esp_err_t e = nvs_open("settings", NVS_READONLY, &nvshandle);
  if (e != ESP_OK) {
    ESP_LOGE("console.c", "Fatal: Failed to open settings in flash for reading: %s", esp_err_to_name(e));
    return 1;
  }
  /* Try reading it as a string first */
  size_t l = 200; char tmpstr[l];
  memset(tmpstr, 0, l);
  e = nvs_get_str(nvshandle, argv[1], &tmpstr[0], &l);
  if (e == ESP_OK) {
    ESP_LOGI("console.c", "STR: %s", tmpstr);
  } else {
    uint8_t v = 0;
    e = nvs_get_u8(nvshandle, argv[1], &v);
    if (e == ESP_OK) {
      ESP_LOGI("console.c", "U08: %u", v);
    } else {
      ESP_LOGE("console.c", "Failed: Could not read string or uint8 with that name.");
    }
  }
  nvs_close(nvshandle);
  return 0;
}

static int console_cmd_nvs_set_str(int argc, char ** argv) {
  if (argc != 3) {
    ESP_LOGE("console.c", "nvs_set_str needs exactly two arguments: settingname and value.");
    ESP_LOGE("console.c", "Example: nvs_set_str wifi_cl_ssid mySSID");
    return 1;
  }
  nvs_handle_t nvshandle;
  esp_err_t e = nvs_open("settings", NVS_READWRITE, &nvshandle);
  if (e != ESP_OK) {
    ESP_LOGE("console.c", "Fatal: Failed to open settings in flash for writing: %s", esp_err_to_name(e));
    return 1;
  }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
  nvs_type_t et;
  // nvs_find_key only exists from ESP-IDF 5.2.X
  e = nvs_find_key(nvshandle, argv[1], &et);
  if ((e == ESP_OK) && (et != NVS_TYPE_STR)) {
    ESP_LOGE("console.c", "Fatal: a setting '%s' already exists in Flash, but it is NOT a string.", argv[1]);
    nvs_close(nvshandle);
    return 1;
  }
#endif /* ESP-IDF >= 5.2.0 */
  /* Sanity checks have passed, lets try to save this setting. */
  e = nvs_set_str(nvshandle, argv[1], argv[2]);
  if (e != ESP_OK) {
    ESP_LOGE("console.c", "Writing the setting to flash failed: %s", esp_err_to_name(e));
  } else {
    e = nvs_commit(nvshandle);
    if (e != ESP_OK) {
      ESP_LOGE("console.c", "Commiting the setting to flash failed: %s", esp_err_to_name(e));
    } else {
      ESP_LOGI("console.c", "Success.");
    }
  }
  nvs_close(nvshandle);
  return 0;
}

static int console_cmd_nvs_set_u8(int argc, char ** argv) {
  if (argc != 3) {
    ESP_LOGE("console.c", "nvs_set_u8 needs exactly two arguments: settingname and value.");
    ESP_LOGE("console.c", "Example: nvs_set_u8 wifi_mode 1");
    return 1;
  }
  nvs_handle_t nvshandle;
  esp_err_t e = nvs_open("settings", NVS_READWRITE, &nvshandle);
  if (e != ESP_OK) {
    ESP_LOGE("console.c", "Fatal: Failed to open settings in flash for writing: %s", esp_err_to_name(e));
    return 1;
  }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
  nvs_type_t et;
  // nvs_find_key only exists from ESP-IDF 5.2.X
  e = nvs_find_key(nvshandle, argv[1], &et);
  if ((e == ESP_OK) && (et != NVS_TYPE_U8)) {
    ESP_LOGE("console.c", "Fatal: a setting '%s' already exists in Flash, but it is NOT an uint8.", argv[1]);
    nvs_close(nvshandle);
    return 1;
  }
#endif /* ESP-IDF >= 5.2.0 */
  /* Sanity checks have passed, lets try to save this setting. */
  long newv = strtol(argv[2], NULL, 10);
  e = nvs_set_u8(nvshandle, argv[1], newv);
  if (e != ESP_OK) {
    ESP_LOGE("console.c", "Writing the setting to flash failed: %s", esp_err_to_name(e));
  } else {
    e = nvs_commit(nvshandle);
    if (e != ESP_OK) {
      ESP_LOGE("console.c", "Commiting the setting to flash failed: %s", esp_err_to_name(e));
    } else {
      ESP_LOGI("console.c", "Success.");
    }
  }
  nvs_close(nvshandle);
  return 0;
}

void console_init(void)
{
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.prompt = "root@foxesptemp#";
  /* FIXME? Do we need longer commands? */
  repl_config.max_cmdline_length = 80;
  /* This registers a 'help' command that will
   * automatically display all available commands.
   * We might replace this with our own help command. */
  esp_console_register_help_command();
  const esp_console_cmd_t cmdrest = {
    .command = "restart",
    .help = "executes esp_restart() to reboot the microcontroller",
    .hint = NULL,
    .func = &console_cmd_restart
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmdrest));
  const esp_console_cmd_t cmdfactoryreset = {
    .command = "factory_reset",
    .help = "Does a factory-reset, i.e. deletes the partition containing the configuration in flash memory.",
    .hint = NULL,
    .func = &console_cmd_factory_reset
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmdfactoryreset));
  const esp_console_cmd_t cmdnvslist = {
    .command = "nvs_list",
    .help = "Lists which settings are in flash (but not what values they are set to)",
    .hint = NULL,
    .func = &console_cmd_nvs_list
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmdnvslist));
  const esp_console_cmd_t cmdnvsget = {
    .command = "nvs_get",
    .help = "Get a setting from flash",
    .hint = "settingname",
    .func = &console_cmd_nvs_get
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmdnvsget));
  const esp_console_cmd_t cmdnvssetstr = {
    .command = "nvs_set_str",
    .help = "Writes a string setting to flash. Note that settings are only read on boot, so this will only take effect on the next reboot.",
    .hint = "settingname value",
    .func = &console_cmd_nvs_set_str
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmdnvssetstr));
  const esp_console_cmd_t cmdnvssetu8 = {
    .command = "nvs_set_u8",
    .help = "Writes a uint8 setting to flash. Note that settings are only read on boot, so this will only take effect on the next reboot. Also note that there are ZERO sanity checks done in this function.",
    .hint = "settingname value",
    .func = &console_cmd_nvs_set_u8
  };
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmdnvssetu8));
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
  esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
  esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#else
  /* We should normally not get here, unless we messed up the other ifdef above. */
#error "Unsupported console type"
#endif /* Initialize the correct type of console */
  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

#else /* any console type is defined */
#warning "No supported console type is defined in sdkconfig, so console support will not be compiled in."

void console_init(void)
{
  ESP_LOGW("console.c", "console support not compiled in, no console available.");
}

#endif /* any console type is defined */

