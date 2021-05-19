#include "Bath.h"
#include "jsmn.h"

/*
* преобразовать строку json в пару name/val
*/

int json_to_str_parm(char *jsonstr, char *nameStr, char *valStr)
{
    int r; // количество токенов
    jsmn_parser p;
    jsmntok_t t[5]; // только 2 пары параметров и obj

    jsmn_init(&p);
    r = jsmn_parse(&p, jsonstr, strlen(jsonstr), t, sizeof(t) / sizeof(t[0]));
    if (r < 0)
        return ESP_FAIL; // не строка jSON

    strncpy(nameStr, jsonstr + t[2].start, t[2].end - t[2].start);
    strncpy(valStr, jsonstr + t[4].start, t[4].end - t[4].start);
    return ESP_OK;
}

/*
*   записать данные из сокета в таблицу параметров wifi
*   если получен WIFI_TAB_RESTART =  true - записать данные в нвс и перезагрузить контроллер
*   если получен WIFI_TAB_RESTART =  false - записать данные в нвс и вернуть эхо всех параметров в сокет
*   возвращаем ESP_FAIL если это не json или параметров нет в таблице 
*/

int read_wifiDataParm_from_socket(char *jsonstr)
{
    char name[32];
    char val[32];
    memset(name, 0, sizeof(name));
    memset(val, 0, sizeof(val));

    int idx;
    if (json_to_str_parm(jsonstr, name, val) != ESP_OK)
    {
        return ESP_FAIL; // str no json
    }
    ESP_LOGI("Data Parm", "Write %s name %s %s", jsonstr, name, DATA_PARM_TO_NVS);
    if (strcmp(name, DATA_PARM_TO_NVS) == 0) // записать данные в нфс
    {
        ESP_LOGI("Data Parm", "Write %s", name);
        write_DataParm_to_nvs();
        return ESP_OK;
    }
    for (idx = 0; idx <= WIFI_TAB_RESTART + 1; idx++)
    {
        printf("IDX %d\n",idx);
        if (idx > WIFI_TAB_RESTART)
        {
            return ESP_FAIL; // not found
        }

        xSemaphoreTake(wifiDataParmMutex, portMAX_DELAY);
        if (strcmp(wifiDataParm[idx].name, name) == 0)
        {
            strcpy(wifiDataParm[idx].val, val);
            printf("name %s\n",name);
        }
        xSemaphoreGive(wifiDataParmMutex);

        if ((idx == WIFI_TAB_RESTART)&&(strcmp(wifiDataParm[idx].name, name) == 0))
        {
            if (strcmp(val, "true") == 0)
            {
                // new wifi data, write to nvs & restart
                ESP_LOGI("READ WIFI DATA FROM SOCKET", "RESTART");
                if (write_wifiDataParm_to_nvs() != ESP_OK)
                    ESP_LOGI("READ WIFI DATA FROM SOCKET", "ERR RESTART");
                esp_restart();
            }
            else if (strcmp(val, "false") == 0)
            {
                // new wifi data, write to nvs & send to socket
                ESP_LOGI("READ WIFI DATA FROM SOCKET", "WRITE ONLY");
                if (write_wifiDataParm_to_nvs() != ESP_OK)
                    ESP_LOGI("READ WIFI DATA FROM SOCKET", "ERR WRITE ONLY");
                read_wifiDataParm_from_nvs(); // reread data from nvs
                send_wifiDataParm_to_socket();
            }
        }
    }
    // send only data from IDX to socket ?? - echo  - не нужен если всегда получаем пакет обновления данных wifi
    return ESP_OK;
}
/*
* read all data from nvs to wifiDataParm
*/
int read_wifiDataParm_from_nvs()
{
    nvs_handle_t my_handle;
    size_t required_size;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI("READ NVS ", "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    }
    for (int idx = 0; idx < WIFI_TAB_RESTART; idx++)
    {
        xSemaphoreTake(wifiDataParmMutex, portMAX_DELAY);
        xSemaphoreTake(NvsMutex, portMAX_DELAY);
        err = nvs_get_str(my_handle, wifiDataParm[idx].name, wifiDataParm[idx].val, &required_size);
        xSemaphoreGive(NvsMutex);
        xSemaphoreGive(wifiDataParmMutex);
        if (err != ESP_OK) // при инициализации всегда ошибки
        {
            ESP_LOGI("READ NVS ", "Error (%s) read NVS data!\n", esp_err_to_name(err));
        }
    }
    nvs_close(my_handle);
    return ESP_OK;
}
/*
* write all data from  wifiDataParm to nvs
*/
int write_wifiDataParm_to_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    }
    for (int idx = 0; idx < WIFI_TAB_RESTART; idx++)
    {
        xSemaphoreTake(wifiDataParmMutex, portMAX_DELAY);
        xSemaphoreTake(NvsMutex, portMAX_DELAY);
        err = nvs_set_str(my_handle, wifiDataParm[idx].name, wifiDataParm[idx].val);
        xSemaphoreGive(NvsMutex);
        xSemaphoreGive(wifiDataParmMutex);
        if (err != ESP_OK) // при инициализации всегда ошибки
        {
            printf("Error (%s) write NVS data!\n", esp_err_to_name(err));
        }
    }
    nvs_close(my_handle);
    return ESP_OK;
}
int write_DataParm_to_nvs()
{
    nvs_handle_t my_handle;
    uint16_t data;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    }
    for (int idx = IDX_BATHVENTUSEHUM; idx < MAX_IDX_PARM_TABLE; idx++)
    {
        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        xSemaphoreTake(NvsMutex, portMAX_DELAY);
        data = (uint16_t)DataParmTable[idx].val;
        err = nvs_set_u16(my_handle, DataParmTable[idx].name, data);
        xSemaphoreGive(NvsMutex);
        xSemaphoreGive(DataParmTableMutex);
        if (err != ESP_OK) // при инициализации всегда ошибки
        {
            printf("Error (%s) write NVS data!\n", esp_err_to_name(err));
        }
    }
    nvs_close(my_handle);
    return ESP_OK;
}
int read_DataParm_from_nvs()
{
    nvs_handle_t my_handle;
    //size_t required_size;
    uint16_t data;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI("READ DATA ", "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    }
    for (int idx = IDX_BATHVENTUSEHUM; idx < MAX_IDX_PARM_TABLE; idx++)
    {
        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        xSemaphoreTake(NvsMutex, portMAX_DELAY);
        err = nvs_get_u16(my_handle, DataParmTable[idx].name, &data);
        if (err == ESP_OK)
        {
            DataParmTable[idx].val = (int)data;
        }
        xSemaphoreGive(NvsMutex);
        xSemaphoreGive(DataParmTableMutex);
        if (err != ESP_OK) // при инициализации всегда ошибки
        {
            ESP_LOGI("READ DATA ", "Error (%s) read NVS data!\n", esp_err_to_name(err));
        }
    }
    nvs_close(my_handle);
    return ESP_OK;
}

/*
* send All data from wifiDataParm то all soskets
*/
void send_wifiDataParm_to_socket()
{
    struct WsDataToSend rq;
    rq.hd = 0;
    rq.fd = 0; // во все сокеты
    rq.idx = 0;
    for (int idx = 0; idx < WIFI_TAB_RESTART; idx++)
    {
        rq.idx = idx + WIFI_TAB_OFFSET;
        xQueueSend(SendWsQueue, &rq, 0);
    }
}
