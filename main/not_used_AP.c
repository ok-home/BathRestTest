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
 
#define SOFT_AP_IP_ADDRESS_1 192
#define SOFT_AP_IP_ADDRESS_2 168
#define SOFT_AP_IP_ADDRESS_3 5
#define SOFT_AP_IP_ADDRESS_4 18
 
#define SOFT_AP_GW_ADDRESS_1 192
#define SOFT_AP_GW_ADDRESS_2 168
#define SOFT_AP_GW_ADDRESS_3 5
#define SOFT_AP_GW_ADDRESS_4 20
 
#define SOFT_AP_NM_ADDRESS_1 255
#define SOFT_AP_NM_ADDRESS_2 255
#define SOFT_AP_NM_ADDRESS_3 255
#define SOFT_AP_NM_ADDRESS_4 0
 
#define SERVER_PORT 3500
#define HTTP_METHOD HTTP_GET	//HTTP_POST
#define URI_STRING "/test"

//	https://esp32.com/viewtopic.php?t=9687
//	https://esp32.com/viewtopic.php?f=2&t=13205

// esp netif object representing the WIFI station
//esp_netif_t *sta_netif = NULL;
esp_netif_t *ap_netif = NULL;

static httpd_handle_t httpServerInstance = NULL;
 
static esp_err_t methodHandler(httpd_req_t* httpRequest){
    //ESP_LOGI("HANDLER","This is the handler for the <%s> URI", httpRequest->uri);
    
    const char* resp_str = "My Test Page";
	httpd_resp_send(httpRequest, resp_str, strlen(resp_str));
	
    return ESP_OK;
}
 
static httpd_uri_t testUri = {
    .uri       = URI_STRING,
    .method    = HTTP_METHOD,
    .handler   = methodHandler,
    .user_ctx  = NULL,
};
 
static void startHttpServer(void){
    httpd_config_t httpServerConfiguration = HTTPD_DEFAULT_CONFIG();
    httpServerConfiguration.server_port = SERVER_PORT;
    if(httpd_start(&httpServerInstance, &httpServerConfiguration) == ESP_OK){
        ESP_ERROR_CHECK(httpd_register_uri_handler(httpServerInstance, &testUri));
    }
}
 
static void stopHttpServer(void){
    if(httpServerInstance != NULL){
        ESP_ERROR_CHECK(httpd_stop(httpServerInstance));
    }
}


static void wifiEventHandler(void* arg, esp_event_base_t event, int32_t event_id, void* event_data)
//static esp_err_t wifiEventHandler(void* userParameter, system_event_t *event)
{
    //switch(event->event_id){
    switch(event_id){
    //case SYSTEM_EVENT_AP_STACONNECTED:
    case WIFI_EVENT_AP_STACONNECTED:
        startHttpServer();
        break;
    //case SYSTEM_EVENT_AP_STADISCONNECTED:
    case WIFI_EVENT_AP_STADISCONNECTED:
        stopHttpServer();
        break;
    default:
        break;
    }
    //return ESP_OK;
}
 
static void launchSoftAp(){
	
    //tcpip_adapter_init();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
	//sta_netif = esp_netif_create_default_wifi_sta();
	//assert( sta_netif );
	
	ap_netif = esp_netif_create_default_wifi_ap();
	assert( ap_netif );
	
	
    //ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    //ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    //tcpip_adapter_ip_info_t ipAddressInfo;
    esp_netif_ip_info_t ipAddressInfo;
    memset(&ipAddressInfo, 0, sizeof(ipAddressInfo));
    IP4_ADDR(
        &ipAddressInfo.ip,
        SOFT_AP_IP_ADDRESS_1,
        SOFT_AP_IP_ADDRESS_2,
        SOFT_AP_IP_ADDRESS_3,
        SOFT_AP_IP_ADDRESS_4);
    IP4_ADDR(
        &ipAddressInfo.gw,
        SOFT_AP_GW_ADDRESS_1,
        SOFT_AP_GW_ADDRESS_2,
        SOFT_AP_GW_ADDRESS_3,
        SOFT_AP_GW_ADDRESS_4);
    IP4_ADDR(
        &ipAddressInfo.netmask,
        SOFT_AP_NM_ADDRESS_1,
        SOFT_AP_NM_ADDRESS_2,
        SOFT_AP_NM_ADDRESS_3,
        SOFT_AP_NM_ADDRESS_4);
    //ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipAddressInfo));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ipAddressInfo));
    //ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    //ESP_ERROR_CHECK(esp_event_loop_init(wifiEventHandler, NULL));
    //ESP_ERROR_CHECK(esp_event_handler_register_with(wifiEventHandler, ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, wifiEventHandler, NULL));
    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_any_id));
    
    wifi_init_config_t wifiConfiguration = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifiConfiguration));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t apConfiguration = {
        .ap = {
            .ssid = SOFT_AP_SSID,
            .password = SOFT_AP_PASSWORD,
            .ssid_len = 0,
            //.channel = default,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 1,
            .beacon_interval = 150,
        },
    };
    //ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apConfiguration));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &apConfiguration));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
}
 
void app_main(void){
    launchSoftAp();
    while(1) vTaskDelay(10);
}