
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
                    break;
                }
                else // включаем автомат - определяем состояние света по датчикам
                {
                    goto auto_on_lbl;
                }
            case IDX_QHD_IrStatus:
            case IDX_QHD_MvStatus:
                if (autolight == 0)
                {
                    break;
                }
            //
            auto_on_lbl:
                IrOnOff = (uint8_t)DataParmTable[IDX_IRVOL].val;
                MvOnOff = (uint8_t)DataParmTable[IDX_MVVOL].val;
                //
                /*   // ИЗМНЕНИЕ ??
                if (unidata.IrData.sender == IDX_QHD_IrStatus)
                    IrOnOff = unidata.IrData.IrStatus;
                else
                    MvOnOff = unidata.MvData.MvStatus;
                */
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
                // сюда ставим информирование о состоянии света и собственно само включение !!
                gpio_set_level(GPIO_BATH_LIGHT_OUT, LightOnOff);
                xQueueSend(BathLightIsrQueue, &LightOnOff, 0); // отправить на обработку включения вентиляции
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_BATHLIGHTSTATUS;
                xQueueSend(SendWsQueue, &req, 0); // отправить http вкл света
                req.idx = IDX_IRVOL;
                xQueueSend(SendWsQueue, &req, 0); // Датчик ИК
                req.idx = IDX_MVVOL;
                xQueueSend(SendWsQueue, &req, 0); // датчик МВ
            }
        }
    }
};
void BathVentControl(void *p)
{
    union QueueHwData unidata;
    int ventOnOff = 0;
    int autoVent = 0;
    int flag;
    struct WsDataToSend req;

    for (;;)
    {
        if (xQueueReceive(BathVentSendToCtrl, &unidata, portMAX_DELAY) == pdTRUE)
        {
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            autoVent = DataParmTable[IDX_BATHVENTAUTOENABLE].val;
            ventOnOff = DataParmTable[IDX_BATHVENTONOFF].val;
            switch (unidata.HttpData.sender)
            {
            case IDX_QHD_HTTP:
                if (autoVent == 0) // ручное управление светом
                {
                    ventOnOff = DataParmTable[IDX_BATHVENTONOFF].val; // ???? лишнее ????
                }
                break;
            case IDX_QHD_HumData:
                if (autoVent == 0)
                {
                    break;
                }
                if (DataParmTable[IDX_BATHVENTUSEHUM].val)
                {
                    ventOnOff = unidata.HumData.HumData;
                    break;
                }
             __attribute__ ((fallthrough));
            case IDX_QHD_LightData:
                if (autoVent == 0)
                {
                    break;
                }
                if (DataParmTable[IDX_BATHVENTUSEMOVE].val)
                {
                    ventOnOff = unidata.LightData.LightData;
                    break;
                }
            }
            flag = 1;
            if (DataParmTable[IDX_BATHVENTSTATUS].val == ventOnOff)
                flag = 0;
            DataParmTable[IDX_BATHVENTSTATUS].val = ventOnOff;
            xSemaphoreGive(DataParmTableMutex);
            if (flag)
            {
                // сюда ставим информирование о состоянии света и собственно само включение !!
                gpio_set_level(GPIO_BATH_VENT_OUT, ventOnOff);

                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_BATHVENTSTATUS;
                xQueueSend(SendWsQueue, &req, 0); // отправить HTTP
                req.idx = IDX_HUMVOL;
                xQueueSend(SendWsQueue, &req, 0); // датчик влажности

                //ESP_LOGI("ВЕНТИЛЯЦИЯ В ВАННОЙ", "Вент %d влажн %d свет %d idx %d", ventOnOff, DataParmTable[IDX_HUMVOL].val, DataParmTable[IDX_BATHLIGHTSTATUS].val, req.idx);
            }
        }
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
        if (xQueueReceive(RestLightSendToCtrl, &unidata, portMAX_DELAY) == pdTRUE)
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
                    break;
                }
                else // вернуться в авторежим
                {
                    if (DataParmTable[IDX_RESTLIGHTONDIST].val < DataParmTable[IDX_DISTVOL].val)
                        {LightOnOff = 0;}
                    else
                        {LightOnOff = 1;} 
                    break;
                }

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
                // сюда ставим информирование о состоянии света и собственно само включени !!
                gpio_set_level(GPIO_REST_LIGHT_OUT, LightOnOff);
                xQueueSend(RestLightIsrQueue, &LightOnOff, 0); // отправить на обработку включения вентиляции

                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_RESTLIGHTSTATUS;
                xQueueSend(SendWsQueue, &req, 0); // Отправить HTTP
                req.idx = IDX_DISTVOL;
                xQueueSend(SendWsQueue, &req, 0); // датчик расстояния
            }
        }
    }
};
void RestVentControl(void *p)
{
    union QueueHwData unidata;

    int ventOnOff = 0;
    int autoVent = 0;
    int flag;
    struct WsDataToSend req;

    for (;;)
    {
        if (xQueueReceive(RestVentSendToCtrl, &unidata, portMAX_DELAY) == pdTRUE)
        {
            xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
            autoVent = DataParmTable[IDX_RESTVENTAUTOENABLE].val;
            ventOnOff = DataParmTable[IDX_RESTVENTONOFF].val;

            switch (unidata.HttpData.sender)
            {
            case IDX_QHD_HTTP:
                if (autoVent == 0) // ручное управление светом
                {
                    ventOnOff = DataParmTable[IDX_RESTVENTONOFF].val; // ???? лишнее ????
                }
                break;
            case IDX_QHD_LightData:
                if (autoVent == 0)
                {
                    break;
                }
                ventOnOff = unidata.LightData.LightData;
            }
            flag = 1;
            if (DataParmTable[IDX_RESTVENTSTATUS].val == ventOnOff)
            {
                flag = 0;
            }
            DataParmTable[IDX_RESTVENTSTATUS].val = ventOnOff;
            xSemaphoreGive(DataParmTableMutex);
            if (flag)
            {

                // сюда ставим информирование о состоянии света и собственно само включение !!
                gpio_set_level(GPIO_REST_VENT_OUT, ventOnOff);

                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_RESTVENTSTATUS;
                xQueueSend(SendWsQueue, &req, 0); // отправить HTTP
            }
        }
    }
};
/*
*  Обработка очереди прерываний от датчиков
*/
void CheckIrMove(void *p)
{
    uint32_t on_off;                     // состояние датчика из очереди обработчика прерывания
    uint32_t MoveDelay;                  // таймаут выключения света ( локально )
    union QueueHwData ud;                // индекс в таблице
    ud.IrData.sender = IDX_QHD_IrStatus; // ик датчик  движения

    MoveDelay = portMAX_DELAY;
    for (;;)
    {
        if (xQueueReceive(IrIsrQueue, &on_off, MoveDelay) == pdTRUE) // получено прерывание
        {
            //ESP_LOGI("Ir isr check", "IrStat %d", on_off);
            if ((on_off == 1) && (MoveDelay == portMAX_DELAY)) // включить свет
            {
                
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_IRVOL].val = 1;
                xSemaphoreGive(DataParmTableMutex);
                ud.IrData.IrStatus = 1; // on
                xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], &ud, 0);
                //    ESP_LOGI("Ir isr ON", "IrStat %d Delay %d", on_off, MoveDelay);
                continue;
            }
            if ((on_off == 0) && (MoveDelay == portMAX_DELAY)) // ждем таймер включения
            {

                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                MoveDelay = (DataParmTable[IDX_BATHLIGHTOFFDELAY].val * 1000) / portTICK_RATE_MS; // в таблице в скундах. Задержка выключения
                xSemaphoreGive(DataParmTableMutex);
                continue;
            }
            if ((on_off == 1) && (MoveDelay != portMAX_DELAY)) // запускаем ожидание выключения
            {
                MoveDelay = portMAX_DELAY;
                continue;
            }
        }
        else // таймаут
        {
            if (on_off == 0) // таймаут - выключаем свет
            {
                
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_IRVOL].val = 0;
                xSemaphoreGive(DataParmTableMutex);
                ud.IrData.IrStatus = 0;
                xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], &ud, 0);
                MoveDelay = portMAX_DELAY;
                //    ESP_LOGI("Ir isr OFF", "IrStat %d Delay %d", on_off, MoveDelay);
                continue;
            }
            else
            {
                ESP_LOGI("Ir Isr Check", "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ");
            }
        }
    }
}

void CheckMvMove(void *p)
{

    uint32_t on_off;                     // состояние датчика из очереди обработчика прерывания
    uint32_t MoveDelay;                  // таймаут выключения света ( локально )
    union QueueHwData ud;                // индекс в таблице
    ud.MvData.sender = IDX_QHD_MvStatus; // ик датчик  движения

    MoveDelay = portMAX_DELAY;
    for (;;)
    {

        if (xQueueReceive(MvIsrQueue, &on_off, MoveDelay) == pdTRUE) // получено прерывание
        {
            //            ESP_LOGI("Mv isr check", "MvStat %d", on_off);
            if ((on_off == 1) && (MoveDelay == portMAX_DELAY)) // включить свет
            {
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_MVVOL].val = 1;
                xSemaphoreGive(DataParmTableMutex);
                                ud.MvData.MvStatus = 1; // on
                xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], &ud, 0);

                //                   ESP_LOGI("MV isr ON", "IrStat %d Delay %d", on_off, MoveDelay);
                continue;
            }
            if ((on_off == 0) && (MoveDelay == portMAX_DELAY)) // ждем таймер включения
            {
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                MoveDelay = (DataParmTable[IDX_BATHLIGHTOFFDELAY].val * 1000) / portTICK_RATE_MS; // в таблице в скундах. Задержка выключения
                xSemaphoreGive(DataParmTableMutex);
                continue;
            }
            if ((on_off == 1) && (MoveDelay != portMAX_DELAY)) // запускаем ожидание выключения
            {
                MoveDelay = portMAX_DELAY;
                continue;
            }
        }
        else // таймаут
        {
            if (on_off == 0) // таймаут - выключаем свет
            {
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_MVVOL].val = 0;
                xSemaphoreGive(DataParmTableMutex);
                ud.MvData.MvStatus = 0; // off
                xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], &ud, 0);
                
                //                   ESP_LOGI("MV isr OFF", "IrStat %d Delay %d", on_off, MoveDelay);
                MoveDelay = portMAX_DELAY;
                continue;
            }
            else
            {
                ESP_LOGI("Ir Isr Check", "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! ");
            }
        }
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
        DataParmTable[IDX_DISTVOL].val = (int)dist;
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
void CheckBathHum(void *p)
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

        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        humOn = DataParmTable[IDX_BATHHUMONPARM].val;
        humOff = DataParmTable[IDX_BATHHUMOFFPARM].val;
        DataParmTable[IDX_HUMVOL].val = (int)hum;
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
            xQueueSend(CtrlQueueTab[Q_BATHVENT_IDX], &ud, 0);
        }
    }
}
/*
*  включение вентиляции относительно включения света в ванной, выключение относительно выключения света в ванной
*  если свет выключен раньше задержки включения - вентиляция не включается
*  порог включения должен быть больше порога выключения
*/
void CheckBathLightOnOff(void *p)
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
        timeout = xQueueReceive(BathLightIsrQueue, &lightOnOff, delay); // включение света
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
            xQueueSend(CtrlQueueTab[Q_BATHVENT_IDX], &ud, 0);
        }
    }
}
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
        timeout = xQueueReceive(RestLightIsrQueue, &lightOnOff, delay); // включение света
        //ESP_LOGI("Light->Vent", "hum %d sender %d", , ud.LightData.sender);
        xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
        ventOnDelay = DataParmTable[IDX_RESTVENTONDELAY].val;
        ventOffDelay = DataParmTable[IDX_RESTVENTOFFDELAY].val;
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
            //            ESP_LOGI("Before Send RestVent -> from Light", "onDelay %d offDelay %d ventOnOff %d", ventOnDelay, ventOffDelay, ventOnOff);
            xQueueSend(CtrlQueueTab[Q_RESTVENT_IDX], &ud, 0);
        }
    }
}

// настроить GPIO на вывод реле управления светом и вентиляцией
void InitOutGPIO()
{
    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_SW_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}