#include "string.h"
 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
 
#define SOFT_AP_SSID "ESP32 SoftAP"
#define SOFT_AP_PASSWORD "Password"
 
#define SERVER_PORT 3500
#define HTTP_METHOD HTTP_GET	//HTTP_POST
#define URI_STRING "/test"

 
static void launchSoftAp(){
	

	esp_netif_t ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifiConfiguration = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifiConfiguration));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t apConfiguration = {
        .ap = {
            .ssid = SOFT_AP_SSID,
            .password = SOFT_AP_PASSWORD,
            .ssid_len = 0,
            .channel = default,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 1,
            .beacon_interval = 150,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &apConfiguration));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
}
 
void app_main(void){
    launchSoftAp();
    while(1) vTaskDelay(10);
}