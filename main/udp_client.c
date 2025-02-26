/*	BSD Socket UDP Client

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h" // IP2STR
#include "lwip/sockets.h"

static const char *TAG = "UDP-CLIENT";

void udp_client(void *pvParameters) {
	ESP_LOGI(TAG, "Start UDP PORT=%d", CONFIG_UDP_PORT);

	/* Get the local IP address */
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));
	ESP_LOGI(TAG, "ip_info.ip="IPSTR, IP2STR(&ip_info.ip));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CONFIG_UDP_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); /* send message to 255.255.255.255 */
	//addr.sin_addr.s_addr = inet_addr("255.255.255.255"); /* send message to 255.255.255.255 */

	// Create a UDP socket
	int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	LWIP_ASSERT("sock >= 0", sock >= 0);

	char buffer[128];
	while(1) {
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
        ESP_LOGI(TAG, "ulTaskNotifyTake");
		int buflen = sprintf(buffer,"take");
		ESP_LOGI(TAG, "buffer=[%s]",buffer);
		int ret = lwip_sendto(sock, buffer, buflen, 0, (struct sockaddr *)&addr, sizeof(addr));
		LWIP_ASSERT("ret == buflen", ret == buflen);
		ESP_LOGI(TAG, "lwip_sendto ret=%d",ret);
	}

	// close socket
	lwip_close(sock);
	vTaskDelete( NULL );
}

