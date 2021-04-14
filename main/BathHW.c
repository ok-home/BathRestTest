#include <esp_log.h>
#include <esp_system.h>
//#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <esp_log.h>

#include "Bath.h"

void BathLightControl(void *p)
{
    union QueueHwData unidata;
    uint8_t IrOnOff = 0;
    uint8_t MvOnOff = 0;
    int LightOnOff = 0;
    int autolight = 0;
    int flag;
    struct WsDataToSend req;

    for (;;)
    {
        if (xQueueReceive(BathLightSendToCtrl, &unidata, portMAX_DELAY) == pdTRUE)
        {
            //определить режим работы автосвета ( автомат/ручное ) и текущее состояние света
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            autolight = DataParmTable[IDX_BATHLIGHTAUTOENABLE].val;
            LightOnOff = DataParmTable[IDX_BATHLIGHTSTATUS].val;

            switch (unidata.HttpData.sender)
            {
            case IDX_QHD_HTTP:
                if (autolight == 0) // ручное управление светом
                {
                    LightOnOff = DataParmTable[IDX_BATHLIGHTONOFF].val;
                }
                break;
            case IDX_QHD_IrStatus:
            case IDX_QHD_MvStatus:
                if (autolight == 0)
                {
                    break;
                }

                if (unidata.IrData.sender == IDX_QHD_IrStatus)
                    IrOnOff = unidata.IrData.IrStatus;
                else
                    MvOnOff = unidata.MvData.MvStatus;

                if (DataParmTable[IDX_BATHLIGHTIRUSE].val && DataParmTable[IDX_BATHLIGHTMVUSE].val)
                {
                    if (IrMvAnyAll == 1)                // all
                        LightOnOff = IrOnOff & MvOnOff; // по срабатыванию обоих датчиков
                    else
                        LightOnOff = IrOnOff | MvOnOff; // по срабатыванию одного из датчиков
                }
                else
                {
                    if (DataParmTable[IDX_BATHLIGHTIRUSE].val)
                        LightOnOff = IrOnOff;
                    if (DataParmTable[IDX_BATHLIGHTMVUSE].val)
                        LightOnOff = MvOnOff;
                }
                break;

            default:
                break;
            }
            flag = 1;
            if (DataParmTable[IDX_BATHLIGHTSTATUS].val == LightOnOff)
                flag = 0;
            DataParmTable[IDX_BATHLIGHTSTATUS].val = LightOnOff;
            xSemaphoreGive(DataParmTableMutex);
            if (flag)
            {
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_BATHLIGHTSTATUS;
                xQueueSend(SendWsQueue, &req, 0);
                //ESP_LOGI("СВЕТ В ВАННОЙ", "Свет %d IR %d Mv %d idx %d\n", LightOnOff, IrOnOff, MvOnOff, req.idx);
                // сюда ставим информирование о состоянии света и собственно само включени !!
            }
        }
    }
};
void BathVentControl(void *p)
{
    union QueueHwData unidata;
    for (;;)
    {
        if (xQueueReceive(BathVentSendToCtrl, &unidata, 1000 / portTICK_PERIOD_MS) == pdTRUE)
            ESP_LOGI("BathVent", "sender %d id %d", unidata.HttpData.sender, unidata.HttpData.ParmIdx);
    }
};
void RestLightControl(void *p)
{
    int LightOnOff = 0;
    int autolight = 0;
    union QueueHwData unidata;
    int flag;
    struct WsDataToSend req;
    for (;;)
    {
        if (xQueueReceive(RestLightSendToCtrl, &unidata, 1000 / portTICK_PERIOD_MS) == pdTRUE)
        {

            //определить режим работы автосвета ( автомат/ручное ) и текущее состояние света
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            autolight = DataParmTable[IDX_RESTLIGHTAUTOENABLE].val;
            LightOnOff = DataParmTable[IDX_RESTLIGHTSTATUS].val;

            switch (unidata.HttpData.sender)
            {
            case IDX_QHD_HTTP:

                if (autolight == 0) // ручное управление светом
                {
                    LightOnOff = DataParmTable[IDX_RESTLIGHTONOFF].val;
                }
                break;
            case IDX_QHD_DistData:
                if (autolight == 0)
                {
                    //ESP_LOGI("BathLight", "DISABLE AUTO LIGHT");
                    break;
                }
                LightOnOff = unidata.DistData.DistStatus;
                break;
            default:
                break;
            }
            //            DataParmTable[IDX_RESTLIGHTSTATUS].val = LightOnOff;
            flag = 1;
            if (DataParmTable[IDX_RESTLIGHTSTATUS].val == LightOnOff)
                flag = 0;
            DataParmTable[IDX_RESTLIGHTSTATUS].val = LightOnOff;
            xSemaphoreGive(DataParmTableMutex);
            if (flag)
            {
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_RESTLIGHTSTATUS;
                xQueueSend(SendWsQueue, &req, 0);
                //ESP_LOGI("СВЕТ В Туалете", "Свет %d idx %d\n", LightOnOff, req.idx);
                // сюда ставим информирование о состоянии света и собственно само включени !!
            }
        }
    }
};
void RestVentControl(void *p)
{
    union QueueHwData unidata;
    for (;;)
    {
        if (xQueueReceive(RestVentSendToCtrl, &unidata, 1000 / portTICK_PERIOD_MS) == pdTRUE)
            ESP_LOGI("RestVent", "sender %d id %d", unidata.HttpData.sender, unidata.HttpData.ParmIdx);
    }
};

/*
*  Обработка очереди прерываний от датчиков
*/
void CheckIrMove(void *p)
{
    uint8_t pp;
    int MoveDelay;
    int ret;
    union QueueHwData *ud = malloc(sizeof(union QueueHwData));
    ud->IrData.sender = IDX_QHD_IrStatus; // ик датчик  движения
    for (;;)
    {
        xQueueReceive(IrIsrQueue, &pp, portMAX_DELAY);
        ud->IrData.IrStatus = 1; // on
        xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], ud, 0);
        ret = pdTRUE;
        while (ret == pdTRUE)
        {
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            MoveDelay = (DataParmTable[IDX_BATHLIGHTOFFDELAY].val * 1000) / portTICK_RATE_MS; // в таблице в скундах. Задержка выключения
            xSemaphoreGive(DataParmTableMutex);
            ret = xQueueReceive(IrIsrQueue, &pp, MoveDelay);
        }
        ud->IrData.IrStatus = 0; // Off
        xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], ud, 0);
    }
}
void CheckMvMove(void *p)
{
    uint8_t pp;
    int MoveDelay;
    int ret;
    union QueueHwData *ud = malloc(sizeof(union QueueHwData));
    ud->IrData.sender = IDX_QHD_MvStatus; // ик датчик  движения
    for (;;)
    {
        xQueueReceive(IrIsrQueue, &pp, portMAX_DELAY);
        ud->IrData.IrStatus = 1; // on
        xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], ud, 0);
        ret = pdTRUE;
        while (ret == pdTRUE)
        {
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            MoveDelay = (DataParmTable[IDX_BATHLIGHTOFFDELAY].val * 1000) / portTICK_RATE_MS; // в таблице в скундах. Задержка выключения
            xSemaphoreGive(DataParmTableMutex);
            ret = xQueueReceive(IrIsrQueue, &pp, MoveDelay);
        }
        ud->IrData.IrStatus = 0; // Off
        xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], ud, 0);
    }
}
/*
*  Включение/вылдючение света по 1 параметру расстояния - дистанция включения
*  Задержка выключения фиксированная - REstLightDelay
*/
void CheckDistMove(void *p)
{
    uint16_t dist;
    int distOn, distDelay, lightOnOff, flag;
    union QueueHwData ud;
    ud.DistData.sender = IDX_QHD_DistData;
    lightOnOff = 0;
    flag = 0;
    distDelay = RestLightDelay;
    for (;;)
    {
        xQueueReceive(DistIsrQueue, &dist, portMAX_DELAY);
        //ESP_LOGI("DIST", "cm %d sender %d", dist, ud.DistData.sender);
        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        distOn = DataParmTable[IDX_RESTLIGHTONDIST].val;
        xSemaphoreGive(DataParmTableMutex);
        if (dist < distOn)
        {
            if (lightOnOff == 0)
            {
                lightOnOff = 1;
                flag = 1;
            }
            else
                flag = 0;
        }
        else
        {
            if (lightOnOff == 1)
            {
                vTaskDelay((distDelay * 1000) / portTICK_RATE_MS);
                lightOnOff = 0;
                flag = 1;
            }
            else
                flag = 0;
        }
        if (flag)
        {
            ud.DistData.DistStatus = lightOnOff;
            flag = 0;
            //   ESP_LOGI("Before Send Rest Light","diston %d dist %d onoff %d ",distOn,dist,lightOnOff );
            xQueueSend(CtrlQueueTab[Q_RESTLIGHT_IDX], &ud, 0);
        }
    }
}

/*
*  Включение/выключение вентиляции по 2 параметрам влажность включения и выключения ( гистерезис )
*  порог включения должен быть больше порога выключения
*/
void CheckRestHum(void *p)
{
    uint16_t hum;
    int humOn, humOff, ventOnOff, flag;
    union QueueHwData ud;
    ud.HumData.sender = IDX_QHD_HumData;
    ventOnOff = 0;
    flag = 0;

    for (;;)
    {
        xQueueReceive(HumIsrQueue, &hum, portMAX_DELAY);
        ESP_LOGI("HUM", "hum %d sender %d", hum, ud.HumData.sender);
        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        humOn = DataParmTable[IDX_BATHHUMONPARM].val;
        humOff = DataParmTable[IDX_BATHHUMOFFPARM].val;
        if (humOn <= humOff)
        {
            humOff = humOn - 1;
            DataParmTable[IDX_BATHHUMOFFPARM].val = humOff;
        }
        xSemaphoreGive(DataParmTableMutex);
        if (ventOnOff == 0)
        {
            if (hum > humOn)
            {
                ventOnOff = 1;
                flag = 1;
            }
        }
        else
        {
            if (hum < humOff)
            {
                ventOnOff = 0;
                flag = 1;
            }
        }
        if (flag)
        {
            ud.HumData.HumData = ventOnOff;
            flag = 0;
            ESP_LOGI("Before Send BathVent", "humon %d humoff %d hum %d ventOnOff %d ", humOn, humOff, hum, ventOnOff);
            xQueueSend(CtrlQueueTab[Q_RESTVENT_IDX], &ud, 0);
        }
    }
}
/*
*  Включение/выключение вентиляции по 2 параметрам задержка включения и выключения - относительно включеня света в ванной
*  включение вентиляции относительно включения света в ванной, выключение относительно выключения света в ванной
*  если свет выключен раньше задержки включения - вентиляция не включается
*  порог включения должен быть больше порога выключения
*/
void CheckRestLightOnOff(void *p)
{
    union QueueHwData ud;
    int lightOnOff = 0;
    int ventOnOff = 0;
    int ventOnDelay, ventOffDelay;
    ud.LightData.sender = IDX_QHD_LightData;
    int delay = portMAX_DELAY; // меняется в зависимости от состояния света
    int timeout = pdTRUE;

    for (;;)
    {
        timeout = xQueueReceive(HumIsrQueue, &lightOnOff, delay); // нужно формировать другую очередь
        //ESP_LOGI("Light->Vent", "hum %d sender %d", , ud.LightData.sender);
        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        ventOnDelay = DataParmTable[IDX_BATHVENTONDELAY].val;
        ventOffDelay = DataParmTable[IDX_BATHVENTOFFDELAY].val;
        xSemaphoreGive(DataParmTableMutex);
        if (timeout == pdTRUE) // есть данные в очереди
        {
            if (lightOnOff == 1) // включили свет - ждем задержку включения вентилятора
            {
                delay = (ventOnDelay * 1000) / portTICK_RATE_MS;
                continue;
            }
            else // выключили свет
            {
                if (ventOnOff == 0) //вентилятор еще не включили в свет уже выключили
                {
                    delay = portMAX_DELAY; // долго ждем очередного включения света
                }
                else // вентилятор включен ждем задержку выключения
                {
                    delay = (ventOffDelay * 1000) / portTICK_RATE_MS;
                }
                continue;
            }
        }
        else // таймаут - сработали задержки включения или выключения
        {
            if (ventOnOff == 0) // вентилятор выключен - включаем
            {
                ventOnOff = 1;
                delay = portMAX_DELAY; // долго ждем выключения света
            }
            else // вентилятор включен - выключаем
            {
                ventOnOff = 0;
                delay = portMAX_DELAY; // долго ждем включения света
            }
            ud.LightData.LightData = ventOnOff;
            ESP_LOGI("Before Send BathVent -> from Light", "onDelay %d offDelay %d ventOnOff %d", ventOnDelay, ventOffDelay, ventOnOff);
            xQueueSend(CtrlQueueTab[Q_RESTVENT_IDX], &ud, 0);
        }
    }
}