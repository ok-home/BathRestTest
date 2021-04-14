#include <stdio.h>
#include <stdlib.h>
#include "jsmn.h"
#include <string.h>
#include <ctype.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include "Bath.h"

/*
*  возвращает индекс в таблице параметров по совпадению имени параметра
*  NameParm - имя параметра
*  если индекс не найден - возвращает -1
*  ищем только индекс - индекс и имя не меняются блокировка не требуется
*/
int FindIdxFromDataParmTable(char *NameParm)
{
    for (int i = 0; i < MAX_IDX_PARM_TABLE; i++)
        if (strcmp(DataParmTable[i].name, NameParm) == 0)
            return (i);
    return (-1);
}

/*
* Преобразование строки Json 2 параметра строка имя, 
* второй параметр только цифра или false/true
* jsonStr - строка Json
* nameStr - возвращаемое имя параметра
* возвращает значение параметра цифра или 0/1 false/true
* -1 - это не строка Json
* -2 - размер nameStr меньше строки наименования параметра
* -3 - параметр не цифра или false/true
*/
int JsonDigitBool(char *jsonStr, char *nameStr, int maxNameSize)
{
    int r; // количество токенов
    jsmn_parser p;
    jsmntok_t t[5]; // только 2 пары параметров и obj

    memset(nameStr, 0, maxNameSize);
    jsmn_init(&p);
    r = jsmn_parse(&p, jsonStr, strlen(jsonStr), t, sizeof(t) / sizeof(t[0]));
    if (r < 0)
        return (-1); // не строка jSON
    if (maxNameSize < t[2].end - t[2].start)
        return (-2); // недостаточная длина строки для имени
    strncpy(nameStr, jsonStr + t[2].start, t[2].end - t[2].start);

    if ((jsonStr + t[4].start)[0] == 't')
        return (1); // true
    if ((jsonStr + t[4].start)[0] == 'f')
        return (0); // false
    if (isdigit((jsonStr + t[4].start)[0]))
        return (atoi(jsonStr + t[4].start)); // число
    return (-3);                             // это не число или false/true
};
/*
* формимирует строку Json из 2-х пар имя:значение
* dest - указатель на строку получатель ( размер не контролируется, может быть превышение)
*/
inline void StrToJson(char *dest, char *name1, char *vol1, char *name2, char *vol2)
{
    strcpy(dest, "{");
    strcat(dest, name1);
    strcat(dest, ":\"");
    strcat(dest, vol1);
    strcat(dest, "\",");
    strcat(dest, name2);
    strcat(dest, ":");
    strcat(dest, vol2);
    strcat(dest, "}");
};
/*
* формирует сроку json из данных таблицы DataParmTable по индексу i
*/
void ParmTableToJson(char *jsonStr, int i)
{
    char valstr[MAXJSONSTRING];
    xSemaphoreTake(DataParmTableMutex, portMAX_DELAY); // блокировка изменения данных таблицы
    if (DataParmTable[i].type == 0)
    {
        if (DataParmTable[i].val)
            strcpy(valstr, "true");
        else
            strcpy(valstr, "false");
    }
    else
        itoa(DataParmTable[i].val, valstr, 10);
    
    StrToJson(jsonStr, "\"name\"", DataParmTable[i].name, "\"msg\"", valstr);
    xSemaphoreGive(DataParmTableMutex); // освободить блокировку
}


/*
* AddSocket, RemSocket, SendSocket - все вызываются только из одного потока SendWsData
* блокировка таблицы сокетов не требуется
*/

// Добавить сокет в таблицу сокетов
inline void AddSocket(struct WsDataToSend req)
{
    for (int flag = 0, sock_idx = 0; sock_idx < CONFIG_LWIP_MAX_SOCKETS; sock_idx++)
    {
        if (SocketArgDb[sock_idx].fd == req.fd) // есть в таблице сокетов
            break;
        if (SocketArgDb[sock_idx].fd == 0) // есть пустая запись в таблице
        {
            if (flag == 0) //такой сокет еще не добавляли
            {
                SocketArgDb[sock_idx].fd = req.fd;
                SocketArgDb[sock_idx].hd = req.hd;
                flag++; // добавили сокет
                continue;
            }
        }
        if (SocketArgDb[sock_idx].fd == req.fd) // проходим дальше конца таблицы, если есть дубль - убираем
        {
            SocketArgDb[sock_idx].fd = 0;
            SocketArgDb[sock_idx].hd = NULL;
            break;
        }
    }
}
// убрать сокет из таблицы сокетов
inline void RemoveSocket(struct WsDataToSend req)
{

    for (int i = 0; i < CONFIG_LWIP_MAX_SOCKETS; i++)
        if (SocketArgDb[i].fd == req.fd)
        { // сокет закрыт - убрать из таблицы
            SocketArgDb[i].fd = 0;
            SocketArgDb[i].hd = NULL;
            break;
        }
}
// отправить в сокет данные из таблицы параметров ( удалиь сокет из таблицы если была ошибка отправки ) 
inline void SendSocket(struct WsDataToSend req)
{
    httpd_ws_frame_t ws_pkt;
    uint8_t buff[64];

    ParmTableToJson((char *)buff, req.idx); // строка данных в HTTP
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buff;
    ws_pkt.len = strlen((char *)buff);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    if (httpd_ws_send_frame_async(req.hd, req.fd, &ws_pkt) != ESP_OK)
    {
        // отправлено с ошибкой - убрать сокет из таблицы сокетов
        RemoveSocket(req);
    }
}
/*
* отправка данных в сокет
* данные из очереди
* req.hd - дескриптор сервера 
* req.fd - номер сокета
* req.idx - индекс таблицы параметров - данные на отправку
* если fd = 0 - отправка по всем сокетам из таблицы сокетов - иначе по конкретному сокету
* если idx < 0 - это стартовый сокет - заполнить таблицу сокетов и отправитоь всю таблицу параметров в данный сокет
* таймаут очереди - если нет данных в очереди - отправлять данные из таблицы параметров по состоянию света
*                   вентиляции и датчиков во все сокеты
*   
*/
void SendWsData(void *p)
{
    struct WsDataToSend req;
    httpd_ws_frame_t ws_pkt;
    uint8_t buff[64];
    int timeout = 5000 / portTICK_RATE_MS); // обновление статусов раз в 5 секунд
    int cntsock = 0;

    for (;;)
    {
        if (xQueueReceive(SendWsQueue, &req, 5000 / portTICK_RATE_MS) == pdTRUE) // до таймаута
        {
            timeout = 5000 / portTICK_RATE_MS); // видимо появился сокет - начинаем обновление статусов
            if (req.idx > MAX_IDX_PARM_TABLE)
                continue; // ошибка данных
            if (req.idx < 0)
            {                   // новый сокет
                AddSocket(req); // добавим сокет в таблицу
                // здесь отправим всю таблицу параметров в этот сокет
                for (int p = 0; p < MAX_IDX_PARM_TABLE; p++)
                {
                    req.idx = p;
                    SendSocket(req);
                }
                continue;
            }

            // req.idx - индекс в таблице параметров

            if (req.fd != 0) // отправить в сокет из запроса
            {
                SendSocket(req);
            }
            else // отправить во все сокеты
            {
                for (int f = 0; f < CONFIG_LWIP_MAX_SOCKETS; f++)
                {
                    if (SocketArgDb[f].fd == 0)
                        continue; // пустой сокет в таблице
                    else
                    {
                        req.fd = SocketArgDb[f].fd;
                        SendSocket(req);
                    }
                }
            }
        }
        else // таймаут - отправляем данные статусов из таблицы
        { 

          for (int  f = 0; f < CONFIG_LWIP_MAX_SOCKETS; f++)
            {
                if (SocketArgDb[f].fd == 0)
                    continue;   // пустой сокет в таблице
                cntsock++;       // есть сокеты на отправку     
             for (int p = 0; p <= IDX_MVVOL; p++)   
                {
                        req.idx = p;   
                        req.fd = SocketArgDb[f].fd;
                        req.hd = SocketArgDb[f].hd;
                        SendSocket(req);
                    
                }
            }
            if(cntsock == 0)
                timeout = portMAX_DELAY; //таблица сокетов пустая, ждем появления хотя бы одного запроса в очереди
        }
    }
}
void CreateTaskAndQueue()
{
    //ограничение доступа к таблице DataParmTable
    DataParmTableMutex = xSemaphoreCreateMutex();
    //ограничение доступа к таблице SocketTable
    SocketTableMutex = xSemaphoreCreateMutex();
    //отправить данные на контроллеры
    //при получении нового сокета
    //NewSockQueue = xQueueCreate(4, sizeof(struct async_resp_arg));
    //обновить все страницы
    //AllSockQueue = xQueueCreate(4, sizeof(struct async_resp_arg));

    BathLightSendToCtrl = xQueueCreate(4, sizeof(union QueueHwData));
    BathVentSendToCtrl = xQueueCreate(4, sizeof(union QueueHwData));
    RestLightSendToCtrl = xQueueCreate(4, sizeof(union QueueHwData));
    RestVentSendToCtrl = xQueueCreate(4, sizeof(union QueueHwData));

    CtrlQueueTab[Q_BATHLIGHT_IDX] = BathLightSendToCtrl;
    CtrlQueueTab[Q_BATHVENT_IDX] = BathVentSendToCtrl;
    CtrlQueueTab[Q_RESTLIGHT_IDX] = RestLightSendToCtrl;
    CtrlQueueTab[Q_RESTVENT_IDX] = RestVentSendToCtrl;

    //очереди из обработчиков прерывания
    IrIsrQueue = xQueueCreate(2, sizeof(uint8_t));
    MvIsrQueue = xQueueCreate(2, sizeof(uint8_t));
    DistIsrQueue = xQueueCreate(5, sizeof(uint16_t)); // длина очереди в секундах
    HumIsrQueue = xQueueCreate(5, sizeof(uint16_t)); // длина очереди в секундах

    // обработчики прерываний
    xTaskCreate(CheckIrMove, "IrMove", 2000, NULL, 1, NULL);
    xTaskCreate(CheckMvMove, "MvMove", 2000, NULL, 1, NULL);
    xTaskCreate(CheckDistMove, "DistMove", 2000, NULL, 1, NULL);

    xTaskCreate(BathLightControl, "blc", 4000, NULL, 1, NULL);
    xTaskCreate(BathVentControl, "bvc", 4000, NULL, 1, NULL);
    xTaskCreate(RestLightControl, "rlc", 4000, NULL, 1, NULL);
    xTaskCreate(RestVentControl, "rvc", 4000, NULL, 1, NULL);

    //заполнить таблицу активных сокетов
    //xTaskCreate(FillSocketTable, "FillSocketTable", 4000, NULL, 1, NULL);
    //обновить страницы активных сокетов
    //xTaskCreate(AllDataSendToWS, "SendStartHTML", 4000, NULL, 1, NULL);

    // очередь отправки в сокет
    SendWsQueue = xQueueCreate(10, sizeof(struct WsDataToSend));
    // отправка в сокет
    xTaskCreate(SendWsData, "Socket Send", 4000, NULL, 1, NULL);

    return;
}
