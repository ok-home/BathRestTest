
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include <esp_http_server.h>
#include <mdns.h>
#include "esp_private/system_internal.h"

#include "Bath.h"
#include "BathInitGlobal.h"
/*
ota test
*/

#define MAX_OTA_SIZE 4096 * 2 + 20
//static uint8_t buff[MAX_OTA_SIZE];
/* A simple example that demonstrates using websocket echo server
 */
static const char *TAG = "ws_echo_server";

static esp_err_t echo_handler(httpd_req_t *req)
{
    union QueueHwData ud;

    struct async_resp_arg resp_arg;
    resp_arg.hd = req->handle;
    resp_arg.fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    struct WsDataToSend rq;
    rq.fd = resp_arg.fd;
    rq.hd = resp_arg.hd;
    char NameField[MAXJSONSTRING];
    httpd_ws_frame_t ws_pkt;

    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    // Set max_len = 0 to get the frame len
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    //ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len)
    {
        // ws_pkt.len + 1 is for NULL termination as we are expecting a string
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        // Set max_len = ws_pkt.len to get the frame payload
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "httpd_ws_recv_frame failed to get frame with %d", ret);
            return ret;
        }
        //ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }

    //    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame echo failed with %d", ret);
        return ret;
    }
    /*
    * если получен стартовый код - новый сокет
    */
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char *)ws_pkt.payload, SOCKSTARTMSG) == 0)
    {
        rq.idx = -1; // занести сокет в таблицу сокетов
        xQueueSend(SendWsQueue, &rq, 0);
        return (ESP_OK);
    }
    /*
    * Определить индекс в таблице параметров(добавить контроль)
    */
    int val = JsonDigitBool((char *)ws_pkt.payload, NameField, sizeof(NameField));
    int idx = FindIdxFromDataParmTable(NameField);
    // данные в таблицу параметров
    xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
    DataParmTable[idx].val = val;
    xSemaphoreGive(DataParmTableMutex);
    /*
    * отправить индекс в таблице параметров обработчикам
    * очередь для обработчика в таблице очередей по индексу из таблицы параметров
    */
    ud.HttpData.sender = 0;
    ud.HttpData.ParmIdx = idx;
    if (DataParmTable[idx].QueueTableIdx >= 0)
    {
        xQueueSend(CtrlQueueTab[DataParmTable[idx].QueueTableIdx], &ud, 0);
    }
    //отправить строку назад - эхо.
    rq.fd = 0; // во все сокеты
    rq.idx = idx;
    xQueueSend(SendWsQueue, &rq, 0);

    return ESP_OK;
}
static esp_err_t ota_handler(httpd_req_t *req)
{
    struct async_resp_arg resp_arg;
    resp_arg.hd = req->handle;
    resp_arg.fd = httpd_req_to_sockfd(req);
    struct WsDataToSend rq;
    rq.fd = resp_arg.fd;
    rq.hd = resp_arg.hd;

    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");

        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    // Set max_len = 0 to get the frame len
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    //ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len)
    {
        // ws_pkt.len + 1 is for NULL termination as we are expecting a string
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        // Set max_len = ws_pkt.len to get the frame payload
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "httpd_ws_recv_frame failed to get frame with %d", ret);
            return ret;
        }
        //ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char *)ws_pkt.payload, "start") == 0) // start ota transfer
    {
        ESP_LOGI("OTA", "START size %d data %s", ws_pkt.len, (char *)ws_pkt.payload);
        ret = start_ota();
        if (ret != ESP_OK)
        {
            ESP_LOGI("START OTA ", "ERR %d", ret);
            return (ret);
        }
        rq.idx = OTA_IDX_MSG_START;
        xQueueSend(SendWsQueue, &rq, 0);
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
             strcmp((char *)ws_pkt.payload, "end") == 0) // stop ota transfer
    {
        ESP_LOGI("OTA", "END size %d data %s", ws_pkt.len, (char *)ws_pkt.payload);
        ret = end_ota();
        if (ret != ESP_OK)
        {
            ESP_LOGI("END OTA ", "ERR %d", ret);
            return (ret);
        }

        rq.idx = OTA_IDX_MSG_END;
        xQueueSend(SendWsQueue, &rq, 0);

        ESP_LOGI(TAG, "Prepare to restart system!");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart_noos_dig();
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_BINARY) //check bin ota data
    {
        //ESP_LOGI("OTA", "DATA BIN type %d size %d", ws_pkt.type, ws_pkt.len);
        ret = write_ota(ws_pkt.len, ws_pkt.payload);
        if (ret == -1)
        {
            ESP_LOGI("WRITE OTA ", "ERR %d", ret);
            return (ret);
        }
        //ESP_LOGI("OTA WS","RET DATA %d",ret);

        rq.idx = OTA_IDX_MSG_NEXT;
        xQueueSend(SendWsQueue, &rq, 0);
    }
    else
    {
        rq.idx = OTA_IDX_MSG_ERR;
        xQueueSend(SendWsQueue, &rq, 0);
        free(buf);
        return ESP_FAIL;
    }
    free(buf);
    return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req)
{

    extern const unsigned char indexbathrest_html_start[] asm("_binary_indexbathrest_html_start");
    extern const unsigned char indexbathrest_html_end[] asm("_binary_indexbathrest_html_end");
    const size_t indexbathrest_html_size = (indexbathrest_html_end - indexbathrest_html_start);

    httpd_resp_send_chunk(req, (const char *)indexbathrest_html_start, indexbathrest_html_size);
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = echo_handler,
    .user_ctx = NULL,
    .is_websocket = true};

static const httpd_uri_t gh = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL};

static const httpd_uri_t ota_ws = {
    .uri = "/otaws",
    .method = HTTP_GET,
    .handler = ota_handler,
    .user_ctx = NULL,
    .is_websocket = true};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &gh);
        httpd_register_uri_handler(server, &ota_ws);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void app_main(void)
{

    static httpd_handle_t server = NULL;

    CreateTaskAndQueue();

    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    //ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    
    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
    /* Start the server for the first time */
    server = start_webserver();

    
}
