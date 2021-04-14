/* WebSocket Echo Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include <esp_http_server.h>

#include "Bath.h"
#include "BathInitGlobal.h"

/* A simple example that demonstrates using websocket echo server
 */
static const char *TAG = "ws_echo_server";
/*
 * async send function, which we put into the httpd work queue
 */
/*static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    return httpd_queue_work(handle, ws_async_send, resp_arg);
}*/

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
    union QueueHwData ud;

    struct async_resp_arg resp_arg;
    resp_arg.hd = req->handle;
    resp_arg.fd = httpd_req_to_sockfd(req);
    struct WsDataToSend rq;
    rq.fd = resp_arg.fd;
    rq.hd = resp_arg.hd;

    char NameField[MAXJSONSTRING];
    uint8_t buf[128] = {0};

    httpd_ws_frame_t ws_pkt;

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    //ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    //ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
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

    ESP_ERROR_CHECK(nvs_flash_init());
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
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    /* Start the server for the first time */
    server = start_webserver();

    testunion();
}
