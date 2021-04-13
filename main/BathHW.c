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
                //ESP_LOGI("BathLight", "HTTP %d id %d", unidata.HttpData.sender, unidata.HttpData.ParmIdx);
                if (autolight == 0) // ручное управление светом
                {
                    LightOnOff = DataParmTable[IDX_BATHLIGHTONOFF].val;
                }
                break;
            case IDX_QHD_IrStatus:
            case IDX_QHD_MvStatus:
                //ESP_LOGI("BathLight", "IrMV %d on/off %d", unidata.IrData.sender, unidata.IrData.IrStatus);
                if (autolight == 0)
                {
                    //ESP_LOGI("BathLight", "DISABLE AUTO LIGHT");
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

                ESP_LOGI("СВЕТ В ВАННОЙ", "Свет %d IR %d Mv %d idx %d\n", LightOnOff, IrOnOff, MvOnOff, req.idx);
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
            //ESP_LOGI("RestLight", "sender %d id %d", unidata.DistData.sender, unidata.DistData.DistStatus);
            //определить режим работы автосвета ( автомат/ручное ) и текущее состояние света
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            autolight = DataParmTable[IDX_RESTLIGHTAUTOENABLE].val;
            LightOnOff = DataParmTable[IDX_RESTLIGHTSTATUS].val;

            switch (unidata.HttpData.sender)
            {
            case IDX_QHD_HTTP:
                //ESP_LOGI("BathLight", "HTTP %d id %d", unidata.HttpData.sender, unidata.HttpData.ParmIdx);
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
                ESP_LOGI("СВЕТ В Туалете", "Свет %d idx %d\n", LightOnOff,  req.idx);
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
