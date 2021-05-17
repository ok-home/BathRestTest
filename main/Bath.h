
#include <esp_log.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <nvs.h>

//#include "jsmn.h"
#include "BathRestHW.h"

#define SOCKSTARTMSG "sstrt" // при получении по WS - новый сокет.
#define MAXJSONSTRING 64 // Буфер JSON

//void testunion();
void CreateTaskAndQueue();
int FindIdxFromDataParmTable(char *);
int JsonDigitBool(char *, char *, int);
void ParmTableToJson(char *, int );
int SendFrameToAllSocket(char *);
void LightISRSetup();

int start_ota(void);
int write_ota(int data_read, uint8_t *ota_write_data);
int end_ota(void);

void SendWsData ( void*p);
QueueHandle_t SendWsQueue;


/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg
{
    httpd_handle_t hd;
    int fd;
};

struct WsDataToSend
{
    httpd_handle_t hd;
    int fd ;
    int idx; // 0-32 индекс, <0 - ничего не отправляем.
};

union QueueHwData
{
    struct HttpData
    {
        int sender; // 0 - send from http
        int ParmIdx;
    } HttpData;
    struct IrData
    {
        int sender; // 1 - send from ir
        int IrStatus;
        //int IrDelay;
    } IrData;
    struct MvData
    {
        int sender; // 2 - send from Mv
        int MvStatus;
        //int MvDelay;
    } MvData;
    struct DistData
    {
        int sender; // 3 - send from dist
        int DistStatus;
        //int DistDelay; // ??
    } DistData;
    struct HumData
    {
        int sender; // 4 - send from dist
        int HumData;
        //int NumDelay; // ??
    } HumData;
    struct LightRestData
    {
        int sender; // 5 - send from dist
        int LightData;
        //int LightDelay; // ??
    } LightData;
};
/*
* union QueueHwData - sender IDX
*/
#define IDX_QHD_HTTP 0     // отправитель HTTP
#define IDX_QHD_IrStatus 1 // отправитель датчик ИК
#define IDX_QHD_MvStatus 2 // отправитель датчик МВ
#define IDX_QHD_DistData 3 // отправитель датчик расстояния
#define IDX_QHD_HumData 4  // отправитель датчик влажности
#define IDX_QHD_LightData 5  // отправитель состояние света в ванной или в туалете ( в разные очереди );


/*
*  Индекс в таблице для параметров
*/
#define IDX_BATHLIGHTSTATUS 0 // состояние света в ванной
#define IDX_RESTLIGHTSTATUS 1 // состояние света в туалете
#define IDX_BATHVENTSTATUS 2  // состояние вентиляции в ванной
#define IDX_RESTVENTSTATUS 3  // сстояние вентиляции в туалете

#define IDX_TEMPVOL 4 // температура с датчика
#define IDX_HUMVOL 5  // влажность с датчика
#define IDX_DISTVOL 6 // расстояние с датчика
#define IDX_IRVOL 7   // состояние ИК датчика движения
#define IDX_MVVOL 8   // состояние МВ датчика движения

#define IDX_BATHLIGHTAUTOENABLE 9  // разрешено автоуправление светом в ванной
#define IDX_RESTLIGHTAUTOENABLE 10 // разрешено автоуправление светом в туалете
#define IDX_BATHVENTAUTOENABLE 11  // разрешено автоуправление вентиляцией в ванной
#define IDX_RESTVENTAUTOENABLE 12  // разрешено автоуправление вентиляцией в туалете

#define IDX_BATHLIGHTONOFF 13 // ручное включение света в ванной
#define IDX_RESTLIGHTONOFF 14 // ручное включение света в туалете
#define IDX_BATHVENTONOFF 15  // ручное включение вентиляции в ванной
#define IDX_RESTVENTONOFF 16  // ручное включение вентиляции в туалете

#define IDX_BATHVENTUSEHUM 17  // используем датчик влажности для автоматики вентиляции в ванной
#define IDX_BATHVENTUSEMOVE 18 // используем датчики движения для автоматики вентиляции в ванной
#define IDX_BATHLIGHTIRUSE 19  // используем  ИК датчик движения для автоматики света в ванной
#define IDX_BATHLIGHTMVUSE 20  // используем  МВ датчик движения для автоматики света в ванной

#define IDX_BATHHUMONPARM 21     // влажность для включения
#define IDX_BATHHUMOFFPARM 22    // влажность для выключения
#define IDX_BATHVENTONDELAY 23   // задержка включения вентиляции в ванной
#define IDX_BATHVENTOFFDELAY 24  // задержка выключения вентиляции в ванной
#define IDX_BATHLIGHTONDELAY 25  // задержка включения света в ванной
#define IDX_BATHLIGHTOFFDELAY 26 // задержка выключения света в ванной
#define IDX_RESTLIGHTONDIST 27   // расстояние включения света в туалете
#define IDX_RESTLIGHTOFFDIST 28  // расстояние выключения света в туалете
#define IDX_RESTVENTONDELAY 29   // задержка включения вентиляции в туалете
#define IDX_RESTVENTOFFDELAY 30   // задержка выключения вентиляции в туалете

#define MAX_IDX_PARM_TABLE 31 // размер таблицы параметров

/*
*  определения индексов для CtrlQueueTab - они же в качестве ссылок в Таблице параметров
*/
#define Q_NO_IDX -1
#define Q_BATHLIGHT_IDX 0
#define Q_BATHVENT_IDX 1
#define Q_RESTLIGHT_IDX 2
#define Q_RESTVENT_IDX 3
#define Q_MAX_TABLE_IDX 4

typedef struct httpNameParm
{
    char *name;        // строка имя в http
    int val;           // значение параметра
    int type;          // тип данных число = 1 false/true =0
    int QueueTableIdx; // индекс в таблице очереди CTRL
} httpNameParm_t;

typedef struct wifiNameParm
{
    char name[32]; // строка имя в http
    char val[32];  // значение параметра
} wifiNameParm_t;



QueueHandle_t BathLightSendToCtrl;
QueueHandle_t BathVentSendToCtrl;
QueueHandle_t RestLightSendToCtrl;
QueueHandle_t RestVentSendToCtrl;
void BathLightControl(void *p);
void BathVentControl(void *p);
void RestLightControl(void *p);
void RestVentControl(void *p);

SemaphoreHandle_t DataParmTableMutex;
SemaphoreHandle_t wifiDataParmMutex;


/*
* Выход обработчиков переывания
*/
xQueueHandle IrIsrQueue; // пррывание от ИК датчика
xQueueHandle MvIsrQueue; // прерывание от МВ датчика
xQueueHandle DistIsrQueue; // прерывания от Датчика расстояния
xQueueHandle HumIsrQueue; // прерывания от Датчика влажности
xQueueHandle BathLightIsrQueue; // прерывания от включения света в ванной
xQueueHandle RestLightIsrQueue; // прерывания от включения света в ванной


void CheckIrMove(void *p); // обработчик ИК датчика 
void CheckMvMove(void *p); // обработчик МВ датчика
void CheckDistMove( void *p); // обработчик датчика расстояния
void CheckBathHum(void *p); // обработчик датчика влажности
void CheckBathLightOnOff(void *p); // обработчик включения света в ванной для вентиляции в ванной
void CheckRestLightOnOff(void *p); // обработчик включения света в туалете для вентиляции в туалете

/*
*  ISR Defifnition
*/
void IrMvISRSetup();
void DistIsrSetup(void *p);
void HumIsrSetup(void *p);

void InitOutGPIO();




/*
*  Глобальные переменные 
*/
extern httpNameParm_t DataParmTable[];
extern wifiNameParm_t wifiDataParm[];

extern struct async_resp_arg SocketArgDb[];
/*
Эти очереди в таблице
QueueHandle_t BathLightSendToCtrl;
QueueHandle_t BathVentSendToCtrl;
QueueHandle_t RestLightSendToCtrl;
QueueHandle_t RestVentSendToCtrl;
*/
extern QueueHandle_t CtrlQueueTab[];

extern uint8_t IrMvAnyAll; // 0 - Any, 1 - All - пока нет в таблице параметров
extern int RestLightDelay;


#define OTA_IDX_MSG_START 64
#define OTA_IDX_MSG_END 65
#define OTA_IDX_MSG_NEXT 66
#define OTA_IDX_MSG_ERR 67

#define WIFI_TAB_OFFSET 40
#define WIFI_TAB_SSID 0
#define WIFI_TAB_PASS 1
#define WIFI_TAB_STA_AP 2
#define WIFI_TAB_RESTART 3


/*
*  wifi nvs definition
*/
int json_to_str_parm(char *, char *, char *);
int read_wifiDataParm_from_socket(char *);
int read_wifiDataParm_from_nvs();
int write_wifiDataParm_to_nvs();
void send_wifiDataParm_to_socket();
