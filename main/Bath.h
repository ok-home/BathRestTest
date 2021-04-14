
#include <esp_log.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>


#define SOCKSTARTMSG "sstrt" // при получении по WS - новый сокет.
#define MAXJSONSTRING 64 // Буфер JSON

void testunion();
void CreateTaskAndQueue();
int FindIdxFromDataParmTable(char *);
int JsonDigitBool(char *, char *, int);
void ParmTableToJson(char *, int );
int SendFrameToAllSocket(char *);

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
        int IrDelay;
    } IrData;
    struct MvData
    {
        int sender; // 2 - send from Mv
        int MvStatus;
        int MvDelay;
    } MvData;
    struct DistData
    {
        int sender; // 3 - send from dist
        int DistStatus;
        int DistDelay; // ??
    } DistData;
    struct HumData
    {
        int sender; // 4 - send from dist
        int HumData;
        int NumDelay; // ??
    } HumData;
    struct LightRestData
    {
        int sender; // 5 - send from dist
        int LightData;
        int LightDelay; // ??
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
#define IDX_RESTVENTFFDELAY 30   // задержка выключения вентиляции в туалете

#define MAX_IDX_PARM_TABLE 31 // размер таблицы параметров

/*
*  опредеения индексов для CtrlQueueTab - они же в качестве ссылок в Таблице параметров
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

QueueHandle_t BathLightSendToCtrl;
QueueHandle_t BathVentSendToCtrl;
QueueHandle_t RestLightSendToCtrl;
QueueHandle_t RestVentSendToCtrl;
void BathLightControl(void *p);
void BathVentControl(void *p);
void RestLightControl(void *p);
void RestVentControl(void *p);

//QueueHandle_t AllSockQueue;
//QueueHandle_t NewSockQueue;
SemaphoreHandle_t SocketTableMutex;
SemaphoreHandle_t DataParmTableMutex;

/*
* Выход обработчиков переывания
*/
xQueueHandle IrIsrQueue; // пррывание от ИК датчика
xQueueHandle MvIsrQueue; // прерывание от МВ датчика
xQueueHandle DistIsrQueue; // прерывания от Датчика расстояния
xQueueHandle HumIsrQueue; // прерывания от Датчика влажности

void CheckIrMove(void *p);
void CheckMvMove(void *p);
void CheckDistMove( void *p);
void CheckRestHum(void *p);

/*
*  Глобальные переменные 
*/
extern httpNameParm_t DataParmTable[];
extern struct async_resp_arg SocketArgDb[];
extern QueueHandle_t *CtrlQueueTab[];

extern uint8_t IrMvAnyAll; // 0 - Any, 1 - All - пока нет в таблице параметров
extern int RestLightDelay;
