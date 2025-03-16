
#include <esp_console.h>
#include <esp_log.h>
#include <esp_system.h>
#include "console.h"

/* Note: REPL = Read Evaluate Print Loop */
esp_console_repl_t * repl = NULL;

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) \
  || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM) \
  || defined(CONFIG_ESP_CONSOLE_USB_CDC)

static int console_cmd_restart(int argc, char ** argv) {
  ESP_LOGI("console.c", "Executing exp_restart()...");
  esp_restart();
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

