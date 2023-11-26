
#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

/* Bits for the network-status event-group */
#define NETWORK_CONNECTED_BIT BIT0  /* Set when we get an IP */

/* You can use xEventGroupWaitBits on this group to wait until
 * bits like the NETWORK_CONNECTED_BIT are set */ 
extern EventGroupHandle_t network_event_group;

/* network_prepare inits the network stack and prepares
 * a connection. Should be called ONLY ONCE. */
void network_prepare(void);
/* Connect to the network. */
void network_on(void);
/* Disconnect from the network. */
void network_off(void);

#endif /* _NETWORK_H_ */

